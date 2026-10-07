#include <iostream>
#include <initializer_list>
#include <stdexcept>

#include "env/env.h"
#include "game/All.h"
#include "game/Commands/CardPileCmd.h"

using namespace sts2;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_crossbow_free_turn_costs_and_x_star_boundary() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "AUDITFREECOST", .character = CharacterId::REGENT},
                    EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    PlayerCombatState pcs = player.PlayerCombatState();
    Creature enemy = CombatState(&env.sim()).Enemies().front();
    pcs.SetEnergy(0);
    pcs.SetStars(3);

    // Crossbow creates a random attack, calls SetToFreeThisTurn, then adds it
    // to the hand. DEVASTATE is a source-defined one-energy, four-star attack.
    CardModel devastate = CombatState(&env.sim()).CreateCard(CardId::DEVASTATE, player);
    CardPileCmd::Add(devastate, PileType::Hand);
    require(devastate.EnergyCost().GetWithModifiers(CostModifiers::All) == 1 &&
                devastate.GetStarCostWithModifiers() == 4 && !devastate.CanPlay(),
            "unmodified Devastate must require one energy and four stars");

    devastate.SetToFreeThisTurn();
    require(devastate.EnergyCost().GetWithModifiers(CostModifiers::All) == 0 &&
                devastate.GetStarCostWithModifiers() == 0 && devastate.CanPlay(),
            "SetToFreeThisTurn must allow four-star Devastate at zero energy and three stars");

    const int32_t enemyHpBefore = enemy.CurrentHp();
    env.Step(Action{.kind = Action::PlayCard, .index = pcs.Hand().Count() - 1, .target = enemy.CombatId()});
    require(pcs.Stars() == 3 && pcs.Energy() == 0,
            "playing free Devastate must spend neither stars nor energy");
    require(!pcs.Hand().Cards().contains(devastate) && pcs.DiscardPile().Cards().contains(devastate),
            "free Devastate did not resolve through the player-play path");
    require(enemy.CurrentHp() < enemyHpBefore,
            "free Devastate did not execute its source-defined attack");
    require(devastate.GetStarCostWithModifiers() == 4,
            "the this-turn star override must also clear when the card is played");

    // A card still in hand keeps both free overrides through the current turn;
    // PlayerCombatState's end-of-turn cleanup removes them and restores source costs.
    CardModel heldDevastate = CombatState(&env.sim()).CreateCard(CardId::DEVASTATE, player);
    CardPileCmd::Add(heldDevastate, PileType::Hand);
    heldDevastate.SetToFreeThisTurn();
    require(heldDevastate.GetStarCostWithModifiers() == 0 &&
                heldDevastate.EnergyCost().GetWithModifiers(CostModifiers::All) == 0,
            "held Devastate was not free during the turn");
    pcs.EndOfTurnCleanup();
    require(heldDevastate.GetStarCostWithModifiers() == 4 &&
                heldDevastate.EnergyCost().GetWithModifiers(CostModifiers::All) == 1,
            "end-of-turn cleanup did not restore Devastate's four-star and one-energy costs");

    // CardModel.GetStarCostWithModifiers returns the current star pool for X-star
    // cards before consulting temporary fixed costs; freeing such a card must not
    // turn its X cost into a zero-star cost.
    pcs.SetStars(3);
    CardModel stardust = CombatState(&env.sim()).CreateCard(CardId::STARDUST, player);
    CardPileCmd::Add(stardust, PileType::Hand);
    require(stardust.HasStarCostX(), "Stardust must be an X-star card");
    stardust.SetToFreeThisTurn();
    require(stardust.GetStarCostWithModifiers() == 3 && stardust.CanPlay(),
            "SetToFreeThisTurn must preserve Stardust's source-defined X-star spend");
}

void test_bullet_time_frees_star_costs_through_play() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "COVFREE0930A", .character = CharacterId::REGENT},
                    EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    PlayerCombatState pcs = player.PlayerCombatState();
    for (CardModel card : pcs.Hand().Cards()) CardPileCmd::Add(card, PileType::Discard);
    CardModel bulletTime = CombatState(&env.sim()).CreateCard(CardId::BULLET_TIME, player);
    CardModel devastate = CombatState(&env.sim()).CreateCard(CardId::DEVASTATE, player);
    CardPileCmd::Add(bulletTime, PileType::Hand);
    CardPileCmd::Add(devastate, PileType::Hand);
    pcs.SetEnergy(3);
    pcs.SetStars(7);

    env.Step(Action{.kind = Action::PlayCard, .index = 0, .target = -1});
    // Source BulletTime.OnPlay calls SetToFreeThisTurn on each non-energy-X
    // card. The original DLL regression retains all seven stars after Devastate.
    require(devastate.CanPlay(), "Bullet Time did not make Devastate playable at zero energy");
    env.Step(Action{.kind = Action::PlayCard, .index = 0,
                    .target = CombatState(&env.sim()).Enemies().front().CombatId()});
    require(pcs.Stars() == 7 && pcs.Energy() == 0,
            "Bullet Time's free Devastate spent resources unlike the original DLL");
    require(devastate.GetStarCostWithModifiers() == 4,
            "Bullet Time's star override did not clear after playing Devastate");
}

void test_duplicate_star_overrides_preserve_lifetime_and_order() {
    struct Case {
        const char* name;
        std::initializer_list<TemporaryCardCost> initial;
        int afterTurn;
    };
    // CardModel's source CurrentStarCost reads the last override, while
    // GetStarCostThisCombat reads the first entry with neither expiry flag.
    // Distinct earlier entries must survive; identical neighbours all expire
    // together under source RemoveAll predicates, including upgrade filtering.
    const Case cases[] = {
        {"no prior override", {}, 4},
        {"combat-long zero", {{0, false, false}}, 0},
        {"different combat values", {{3, false, false}, {1, false, false}}, 1},
        {"until played", {{2, false, true}}, 2},
    };
    for (const Case& item : cases) {
        Env env;
        env.ResetCombat(NewRunOptions{.seed = "COVCAP0930", .character = CharacterId::REGENT},
                        EncounterId::CULTISTS_NORMAL);
        Player player(&env.sim(), 1);
        CardModel card = CombatState(&env.sim()).CreateCard(CardId::DEVASTATE, player);
        CardPileCmd::Add(card, PileType::Hand);
        for (const auto& cost : item.initial) card.st().temporary_star_costs.push_back(cost);
        for (int repeat = 0; repeat < 4; ++repeat) card.SetToFreeThisTurn();
        require(card.CurrentStarCost() == 0, item.name);
        require(card.st().temporary_star_costs.size() == item.initial.size() + 4,
                "star overrides did not append every original list entry");
        size_t index = 0;
        for (const auto& cost : item.initial) {
            const auto& kept = card.st().temporary_star_costs[index++];
            require(kept.cost == cost.cost &&
                        kept.clears_when_turn_ends == cost.clears_when_turn_ends &&
                        kept.clears_when_card_is_played == cost.clears_when_card_is_played,
                    "appending changed an earlier value or lifetime");
        }
        card.EndOfTurnCleanup();
        require(card.CurrentStarCost() == item.afterTurn,
                "turn expiry changed the source last-override result");
    }

    Env env;
    env.ResetCombat(NewRunOptions{.seed = "COVCAPCOMBAT0930", .character = CharacterId::REGENT},
                    EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    CardModel card = CombatState(&env.sim()).CreateCard(CardId::DEVASTATE, player);
    CardPileCmd::Add(card, PileType::Hand);
    for (int repeat = 0; repeat < 4; ++repeat) card.SetToFreeThisCombat();
    card.EndOfTurnCleanup();
    require(card.CurrentStarCost() == 0 && card.st().temporary_star_costs.size() == 4,
            "identical combat-long overrides did not retain their source lifetime");
}

}  // namespace

int main() {
    test_crossbow_free_turn_costs_and_x_star_boundary();
    test_bullet_time_frees_star_costs_through_play();
    test_duplicate_star_overrides_preserve_lifetime_and_order();
    std::cout << "free-cost audit passed\n";
}
