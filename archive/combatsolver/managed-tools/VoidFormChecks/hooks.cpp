// Independent counterpart to Program.cs. Expected values come from the
// shipped TestMode=false C# hooks; compare the two emitted JSON documents.
#include <iostream>

#include "engine/json.h"
#include "engine/sim/alloc.h"
#include "env/env.h"
#include "game/All.h"
#include "game/Entities/Cards/CardPile.h"
#include "game/Models/Powers/VoidFormPower.h"

using namespace sts2;

int main() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "VOIDHOOK", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    Sim* sim = &env.sim();
    Player player(sim, 1);
    Creature enemy = CombatState(sim).Enemies().front();
    PlayerChoiceContext context = PlayerChoiceContext::Throwing();
    CardModel card = CombatState(sim).CreateCard(CardId::STRIKE_IRONCLAD, player);
    CardPile::Get(PileType::Hand, player).AddInternal(card, -1, true);
    VoidFormPower power(sim, alloc_power(env.state(), PowerId::VOID_FORM_POWER));
    power.ApplyInternal(player.Creature(), Dec(2), true);
    VoidFormPower enemy_power(sim, alloc_power(env.state(), PowerId::VOID_FORM_POWER));
    enemy_power.ApplyInternal(enemy, Dec(2), true);
    PowerModel unrelated(sim, alloc_power(env.state(), PowerId::STRENGTH_POWER));
    unrelated.ApplyInternal(player.Creature(), Dec(1), true);
    JsonWriter writer;
    writer.begin_object().key("rows").begin_array();
    auto bits = [&](const char* key, Dec cost) {
        writer.key(key).begin_array();
        for (uint32_t word : {cost.lo, cost.mid, cost.hi, cost.flags}) writer.value(uint64_t(word));
        writer.end_array();
    };
    auto record = [&](const char* name, VoidFormPower queried) {
        Dec energy, stars;
        bool changed_energy = queried.TryModifyEnergyCostInCombatLate(card, Dec::from_bits(712500, 0, 0, 5U << 16), energy);
        bool changed_stars = queried.TryModifyStarCost(card, Dec::from_bits(4000, 0, 0, 3U << 16), stars);
        writer.begin_object().field("name", name).field("counter", power.CardsPlayedThisTurn())
            .field("energy_modified", changed_energy).field("star_modified", changed_stars);
        bits("energy_bits", energy);
        bits("star_bits", stars);
        writer.end_object();
    };
    auto play = [&](bool automatic, int index, int count) {
        CardPlay card_play{.Card = card, .Player = player, .IsAutoPlay = automatic, .PlayIndex = index, .PlayCount = count};
        power.AfterCardPlayed(context, &card_play);
    };
    record("initial", power);
    power.BeforeApplied(player.Creature(), Dec(2), player.Creature(), card);
    record("before_applied", power);
    power.BeforeSideTurnStart(context, CombatSide::Enemy, CreatureList{enemy}, CombatState(sim));
    record("other_participant", power);
    power.BeforeSideTurnStart(context, CombatSide::Player, CreatureList{player.Creature()}, CombatState(sim));
    record("owner_participant", power);
    for (auto [pile, name] : {std::pair{PileType::Play, "pile_play"}, {PileType::Draw, "pile_draw"},
                              {PileType::Discard, "pile_discard"}, {PileType::Exhaust, "pile_exhaust"},
                              {PileType::Hand, "pile_hand"}}) {
        card.Pile().RemoveInternal(card, true);
        CardPile::Get(pile, player).AddInternal(card, -1, true);
        record(name, power);
    }
    record("foreign_owner", enemy_power);
    play(true, 0, 1); record("automatic", power);
    play(false, 0, 2); record("manual_nonlast", power);
    play(false, 1, 2); record("manual_last", power);
    play(false, 0, 1); record("manual_threshold", power);
    power.BeforePowerAmountChanged(unrelated, Dec(1), player.Creature(), player.Creature(), card);
    record("unrelated_amount", power);
    power.BeforeSideTurnStart(context, CombatSide::Player, CreatureList{player.Creature()}, CombatState(sim));
    power.BeforePowerAmountChanged(power, Dec(1), player.Creature(), player.Creature(), card);
    record("own_amount", power);
    writer.end_array().field("mapped_counter", power.CardsPlayedThisTurn()).field("test_mode", false).end_object();
    std::cout << writer.str() << '\n';
}
