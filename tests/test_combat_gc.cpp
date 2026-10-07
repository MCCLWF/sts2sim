#include <iostream>
#include <stdexcept>

#include "engine/run_setup.h"
#include "engine/sim/alloc.h"
#include "engine/sim/gc.h"
#include "env/env.h"
#include "game/All.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Commands/RelicCmd.h"

using namespace sts2;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void fill_retired_powers(GameState& g) {
    while (g.powers.size() < kPowerPool - 16) alloc_power(g, PowerId::WEAK_POWER);
}

void test_repeated_power_expiration_does_not_exhaust_pool() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "AUDITPOWERGC", .character = CharacterId::IRONCLAD},
                    EncounterId::CULTISTS_NORMAL);
    Sim& sim = env.sim();
    Creature player = Player(&sim, 1).Creature();
    Creature enemy = CombatState(&sim).Enemies().front();
    PlayerChoiceContext context;
    const int initial_powers = sim.g->powers.size();
    const auto initial_rng = sim.g->run.rng;
    for (int cycle = 0; cycle < 1000; ++cycle) {
        PowerModel weak = PowerCmd::Apply(PowerId::WEAK_POWER, context, player, Dec(1), enemy, {});
        require(static_cast<bool>(weak), "Weak application failed");
        PowerCmd::Remove(weak);
        require(sim.g->powers.live(weak.Index()), "power freed before hooks completed");
        CollectDetachedCreatures(*sim.g); // The same completed-action boundary used by Env::Run.
        require(!player.HasPower(PowerId::WEAK_POWER), "removed Weak remained attached");
    }
    require(sim.g->powers.size() < kPowerPool - 16, "retired powers accumulated past the sweep threshold");
    require(sim.g->powers.size() >= initial_powers, "active encounter powers were collected");
    require(sim.g->run.rng == initial_rng, "power collection advanced run RNG");
    auto restored = Env::ImportSnapshotJson(env.ExportSnapshotJson());
    require(restored->Hash() == env.Hash(), "collected state failed snapshot round trip");
}

void test_repeated_affliction_expiration_does_not_exhaust_pool() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "AUDITAFFLICTIONGC", .character = CharacterId::IRONCLAD},
                    EncounterId::CULTISTS_NORMAL);
    Sim& sim = env.sim();
    Player player(&sim, 1);
    CombatState combat(&sim);
    CardModel card = combat.CreateCard(CardId::STRIKE_IRONCLAD, player);
    CardPileCmd::Add(card, PileType::Hand);
    for (int cycle = 0; cycle < 1000; ++cycle) {
        AfflictionModel applied = CardCmd::Afflict(AfflictionId::RINGING, card, Dec(1));
        require(applied && card.Affliction(), "Ringing application failed");
        CardCmd::ClearAffliction(card);
        CollectDetachedCreatures(*sim.g); // same completed-action boundary as Env::Run
    }
    require(sim.g->afflictions.size() < kAfflictionPool - 16,
            "detached AfflictionModels accumulated past the sweep threshold");
}

void test_retained_power_and_creature_references_survive() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "AUDITGCRETENTION", .character = CharacterId::IRONCLAD},
                    EncounterId::CULTISTS_NORMAL);
    Sim& sim = env.sim();
    GameState& g = *sim.g;
    Player player(&sim, 1);
    Creature enemy = CombatState(&sim).Enemies().front();
    PlayerChoiceContext context;
    PowerModel remembered = PowerCmd::Apply(PowerId::WEAK_POWER, context, enemy, Dec(2), player.Creature(), {});
    PowerCmd::Remove(remembered);
    RelicModel lamp = RelicCmd::Obtain(RelicId::UNSETTLING_LAMP, player);
    g.relics[lamp.Index()].extra[1] = remembered.Index();

    CreatureRef detached = g.creatures.alloc();
    CreatureRef target = g.creatures.alloc();
    CreatureRef caster = g.creatures.alloc();
    PowerRef captured = alloc_power(g, PowerId::DAMPEN_POWER);
    g.powers[captured].owner = detached;
    g.powers[captured].applier = player.Creature().Index();
    g.powers[captured].target = target;
    g.powers[captured].dampen_casters.push_back(caster);
    g.creatures[detached].powers.push_back(captured);
    g.combat.active_card_play_power_captures.push_back({captured, player.Deck().Cards().front().Index(), 1});
    fill_retired_powers(g);
    g.combat.executing_card_or_potion_effect = 1;
    require(CollectDetachedCreatures(g) == 0, "collection ran inside a card effect");
    require(g.powers.size() == kPowerPool - 16, "in-flight power slots were reclaimed");
    g.combat.executing_card_or_potion_effect = 0;
    require(CollectDetachedCreatures(g) > 0, "full power pool did not trigger collection");
    require(g.powers.live(remembered.Index()), "Unsettling Lamp's remembered retired power was freed");
    require(g.powers.live(captured) && g.creatures.live(detached), "active capture lost its power or owner");
    require(g.creatures.live(target) && g.creatures.live(caster), "retained power lost its target or Dampen caster");
    g.combat.active_card_play_power_captures.clear();
    g.relics[lamp.Index()].extra[1] = 0;
    fill_retired_powers(g);
    CollectDetachedCreatures(g);
    require(!g.powers.live(remembered.Index()), "unreferenced retired power remained allocated");
    require(!g.powers.live(captured) && !g.creatures.live(detached), "unreferenced detached power/owner cycle survived");
    require(!g.creatures.live(target) && !g.creatures.live(caster), "unreferenced target/caster survived");
}

void test_reward_and_merchant_afflictions_are_strong_roots() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "AUDITAFFLICTIONROOTS", .character = CharacterId::IRONCLAD},
                    EncounterId::CULTISTS_NORMAL);
    Sim& sim = env.sim();
    GameState& g = *sim.g;
    const CardRef reward_card = alloc_card(g, CardId::STRIKE_IRONCLAD, 1);
    const CardRef merchant_card = alloc_card(g, CardId::DEFEND_IRONCLAD, 1);
    CardModel reward_model(&sim, reward_card), merchant_model(&sim, merchant_card);
    reward_model.AfflictInternal(AfflictionModel::Pending(&sim, AfflictionId::RINGING), Dec(2));
    merchant_model.AfflictInternal(AfflictionModel::Pending(&sim, AfflictionId::RINGING), Dec(3));
    const AfflictionRef reward_affliction = g.cards[reward_card].affliction_instance;
    const AfflictionRef merchant_affliction = g.cards[merchant_card].affliction_instance;
    const RewardRef reward = g.rewards.alloc();
    g.rewards[reward].type = RewardType::Card;
    g.rewards[reward].cards.push_back(reward_card);
    g.run.rewards.active = true;
    g.run.rewards.rewards.push_back(reward);
    g.run.merchant.exists = true;
    g.run.merchant.character_cards.push_back({.card = merchant_card});
    g.combat.exists = false; // GC component fixture after the owning combat has ended.
    CollectGarbage(g);
    require(g.cards.live(reward_card) && g.afflictions.live(reward_affliction),
            "reward card's attached AfflictionModel was swept");
    require(g.cards.live(merchant_card) && g.afflictions.live(merchant_affliction),
            "merchant card's attached AfflictionModel was swept");
    g.run.rewards.active = false;
    g.run.merchant.exists = false;
    CollectGarbage(g);
    require(!g.cards.live(reward_card) && !g.afflictions.live(reward_affliction)
                && !g.cards.live(merchant_card) && !g.afflictions.live(merchant_affliction),
            "unrooted cards left dangling AfflictionModel objects");
}
} // namespace

int main() {
    try {
        test_repeated_power_expiration_does_not_exhaust_pool();
        test_repeated_affliction_expiration_does_not_exhaust_pool();
        test_retained_power_and_creature_references_survive();
        test_reward_and_merchant_afflictions_are_strong_roots();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "combat GC regression: " << error.what() << '\n';
        return 1;
    }
}
