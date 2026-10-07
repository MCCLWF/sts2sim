#include <stdexcept>
#include <string>

#include "env/env.h"
#include "engine/sim/alloc.h"
#include "game/All.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/Powers/AfterimagePower.h"
#include "game/Models/Powers/NightmarePower.h"
#include "game/Models/Powers/SerpentFormPower.h"
#include "game/Models/Powers/StranglePower.h"

using namespace sts2;

namespace {

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

struct Fixture {
    Env env;
    Sim* sim;
    Player player;
    CombatState combat;

    Fixture() : sim(nullptr), player(), combat() {
        env.ResetCombat({.seed = "SILENTCAPTURE", .character = CharacterId::SILENT}, EncounterId::CULTISTS_NORMAL);
        sim = &env.sim();
        player = Player(sim, 1);
        combat = CombatState(sim);
    }

    CardModel card(CardId id) {
        CardModel value = combat.CreateCard(id, player);
        CardPileCmd::Add(value, PileType::Hand);
        return value;
    }

    CardPlay play(CardModel value) const {
        return CardPlay{value, player, Creature{}, PileType::Discard, {}, false, 0, 1};
    }
};

void test_afterimage_nested_duplicate_snapshot_and_removal() {
    Fixture f;
    PlayerChoiceContext ctx;
    PowerModel raw = PowerCmd::Apply(PowerId::AFTERIMAGE_POWER, ctx, f.player.Creature(), Dec(2),
                                     f.player.Creature(), CardModel{});
    require(bool(raw), "Afterimage did not apply");
    CardModel first = f.card(CardId::STRIKE_SILENT);
    CardModel second = f.card(CardId::DEFEND_SILENT);
    CardPlay firstPlay = f.play(first);
    CardPlay secondPlay = f.play(second);
    AfterimagePower power(raw);

    power.BeforeCardPlayed(&firstPlay);
    PowerCmd::ModifyAmount(ctx, raw, Dec(3), f.player.Creature(), CardModel{});
    power.BeforeCardPlayed(&secondPlay);

    const std::string pending = f.env.ExportSnapshotJson();
    auto restored = Env::ImportSnapshotJson(pending);
    require(restored->ExportSnapshotJson() == pending, "active Afterimage captures did not round-trip exactly");

    auto finish = [&](Env& env) {
        Sim& sim = env.sim();
        Player owner(&sim, 1);
        CardModel a(&sim, first.Index());
        CardModel b(&sim, second.Index());
        PlayerChoiceContext choice;
        AfterimagePower model(&sim, raw.Index());
        CardPlay pa{a, owner, Creature{}, PileType::Discard, {}, false, 0, 1};
        CardPlay pb{b, owner, Creature{}, PileType::Discard, {}, false, 0, 1};
        model.AfterCardPlayed(choice, &pb);
        model.AfterCardPlayed(choice, &pa);
    };
    finish(f.env);
    finish(*restored);
    require(f.env.Hash() == restored->Hash(), "restored nested Afterimage completion diverged");
    require(f.player.Creature().Block() == 7, "Afterimage did not use the captured per-play amounts");
    require(f.combat.st().active_card_play_power_captures.empty(), "Afterimage left a capture after completion");

    power.BeforeCardPlayed(&firstPlay);
    bool duplicateRejected = false;
    try { power.BeforeCardPlayed(&firstPlay); }
    catch (const GameError&) { duplicateRejected = true; }
    require(duplicateRejected, "duplicate active CardPlay identity was accepted");
    PowerCmd::Remove(raw);
    require(f.combat.st().active_card_play_power_captures.empty(), "removed Afterimage retained active captures");
}

void test_strangle_captures_multiple_instances_for_one_card() {
    Fixture f;
    const CreatureList enemies = f.combat.Enemies();
    require(enemies.size() >= 2, "fixture requires two Strangle targets");
    PlayerChoiceContext ctx;
    PowerModel first = PowerCmd::Apply(PowerId::STRANGLE_POWER, ctx, enemies[0], Dec(2),
                                       f.player.Creature(), CardModel{});
    PowerModel second = PowerCmd::Apply(PowerId::STRANGLE_POWER, ctx, enemies[1], Dec(4),
                                        f.player.Creature(), CardModel{});
    require(first && second && first != second, "fixture did not create two Strangle instances");
    CardModel card = f.card(CardId::STRIKE_SILENT);
    CardPlay cardPlay = f.play(card);
    StranglePower p1(first), p2(second);

    p1.BeforeCardPlayed(&cardPlay);
    p2.BeforeCardPlayed(&cardPlay);
    PowerCmd::ModifyAmount(ctx, first, Dec(3), f.player.Creature(), CardModel{});
    PowerCmd::ModifyAmount(ctx, second, Dec(5), f.player.Creature(), CardModel{});
    const int hp1 = enemies[0].CurrentHp();
    const int hp2 = enemies[1].CurrentHp();
    p1.AfterCardPlayed(ctx, &cardPlay);
    p2.AfterCardPlayed(ctx, &cardPlay);
    require(enemies[0].CurrentHp() == hp1 - 2 && enemies[1].CurrentHp() == hp2 - 4,
            "Strangle instances did not retain their separate pre-play amounts");
    require(f.combat.st().active_card_play_power_captures.empty(), "Strangle left captures after completion");
}

void test_serpent_form_nested_capture() {
    Fixture f;
    PlayerChoiceContext ctx;
    PowerModel raw = PowerCmd::Apply(PowerId::SERPENT_FORM_POWER, ctx, f.player.Creature(), Dec(2),
                                     f.player.Creature(), CardModel{});
    CardModel first = f.card(CardId::STRIKE_SILENT);
    CardModel second = f.card(CardId::DEFEND_SILENT);
    CardPlay firstPlay = f.play(first);
    CardPlay secondPlay = f.play(second);
    SerpentFormPower power(raw);
    power.BeforeCardPlayed(&firstPlay);
    PowerCmd::ModifyAmount(ctx, raw, Dec(3), f.player.Creature(), CardModel{});
    power.BeforeCardPlayed(&secondPlay);
    const size_t before = f.combat.st().active_card_play_power_captures.size();
    require(before == 2, "Serpent Form did not track both nested cards");
    power.AfterCardPlayed(ctx, &secondPlay);
    power.AfterCardPlayed(ctx, &firstPlay);
    require(f.combat.st().active_card_play_power_captures.empty(), "Serpent Form left captures after completion");
}

void test_knife_trap_calculated_exhaust_shiv_count() {
    Fixture f;
    CardModel trap = f.card(CardId::KNIFE_TRAP);
    for (int i = 0; i < 2; ++i) {
        CardModel shiv = f.combat.CreateCard(CardId::SHIV, f.player);
        CardPileCmd::Add(shiv, PileType::Exhaust);
    }
    CardModel strike = f.combat.CreateCard(CardId::STRIKE_SILENT, f.player);
    CardPileCmd::Add(strike, PileType::Exhaust);
    require(trap.DynamicVars()["CalculatedShivs"].Calculate(Creature{}).to_int() == 2,
            "Knife Trap did not count only Shiv-tagged cards in Exhaust");
}

void test_nightmare_card_choice_restores_and_keeps_selected_card() {
    Fixture f;
    f.player.PlayerCombatState().Hand().Clear();
    CardModel selected = f.card(CardId::STRIKE_SILENT);
    f.card(CardId::DEFEND_SILENT);
    CardModel nightmare = f.card(CardId::NIGHTMARE);
    const int32_t handIndex = f.player.PlayerCombatState().Hand().Cards().index_of(nightmare);
    require(handIndex >= 0, "Nightmare fixture card is not in the hand");

    auto countChoice = f.env.Predict(Action{.kind = Action::PlayCard, .index = handIndex});
    require(countChoice->decision().kind == DecisionKind::Choice, "Nightmare did not request its hand selection");
    auto choice = countChoice->Predict(countChoice->LegalActions().front());
    require(choice->decision().kind == DecisionKind::Choice, "Nightmare did not request a card after selector count");
    const std::string snapshot = choice->ExportSnapshotJson();
    const Action answer = choice->LegalActions().front();
    auto expected = choice->Predict(answer);
    auto restored = Env::ImportSnapshotJson(snapshot);
    require(restored->Hash() == choice->Hash(), "Nightmare pending selection snapshot restore differs");
    require(restored->Predict(answer)->Hash() == expected->Hash(), "restored Nightmare selection answer diverged");
    require(snapshot == choice->ExportSnapshotJson(), "answering Nightmare mutated its source choice");

    PowerModel selectedPower = Player(&expected->sim(), 1).Creature().GetPower(PowerId::NIGHTMARE_POWER);
    require(bool(selectedPower), "Nightmare selection did not apply NightmarePower");
    CardModel selectedCopy = NightmarePower(selectedPower).SelectedCard();
    require(selectedCopy && selectedCopy.CardId_() == selected.CardId_(),
            "NightmarePower did not retain a clone of the selected Silent card");
}

}  // namespace

int main() {
    test_afterimage_nested_duplicate_snapshot_and_removal();
    test_strangle_captures_multiple_instances_for_one_card();
    test_serpent_form_nested_capture();
    test_knife_trap_calculated_exhaust_shiv_count();
    test_nightmare_card_choice_restores_and_keeps_selected_card();
}
