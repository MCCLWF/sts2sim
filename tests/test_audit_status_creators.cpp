#include <iostream>
#include <stdexcept>

#include "engine/run_setup.h"
#include "env/env.h"
#include "game/All.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/Monsters/TheInsatiable.h"

using namespace sts2;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    Env env;
    Sim* sim = nullptr;
    Player player;
    CombatState combat;
    Creature insatiable;

    Fixture() {
        env.ResetCombat(NewRunOptions{.seed = "AUDITINSATIABLECREATOR", .character = CharacterId::DEFECT},
                        EncounterId::CULTISTS_NORMAL);
        sim = &env.sim();
        player = Player(sim, 1);
        combat = CombatState(sim);
        insatiable = CreatureCmd::Add(MonsterModel::ToMutable(sim, MonsterId::THE_INSATIABLE),
                                       combat, CombatSide::Enemy);
        require(static_cast<bool>(insatiable), "failed to add The Insatiable to the combat fixture");
    }
};

int count_cards(const CardList& cards, CardId id) {
    int count = 0;
    for (CardModel card : cards) count += card.CardId_() == id;
    return count;
}

void test_insatiable_statuses_keep_player_owner_but_no_player_creator() {
    Fixture f;
    CardModel rocketPunch = f.combat.CreateCard(CardId::ROCKET_PUNCH, f.player);
    CardPileCmd::Add(rocketPunch, PileType::Hand);
    require(rocketPunch.EnergyCost().GetWithModifiers(CostModifiers::All) == 2,
            "Rocket Punch fixture did not start at its canonical cost");

    TheInsatiable(f.insatiable.Monster()).LiquifyMove(CreatureList{f.player.Creature()});

    const CardList draw = f.player.PlayerCombatState().DrawPile().Cards();
    const CardList discard = f.player.PlayerCombatState().DiscardPile().Cards();
    require(count_cards(draw, CardId::FRANTIC_ESCAPE) == 3 &&
                count_cards(discard, CardId::FRANTIC_ESCAPE) == 3,
            "The Insatiable did not place three Frantic Escape cards in each requested pile");
    for (CardModel card : draw) {
        if (card.CardId_() == CardId::FRANTIC_ESCAPE) {
            require(card.Owner() == f.player && card.Type() == CardType::Status,
                    "The Insatiable's draw-pile Frantic Escape has the wrong owner or type");
        }
    }
    for (CardModel card : discard) {
        if (card.CardId_() == CardId::FRANTIC_ESCAPE) {
            require(card.Owner() == f.player && card.Type() == CardType::Status,
                    "The Insatiable's discard-pile Frantic Escape has the wrong owner or type");
        }
    }
    require(rocketPunch.EnergyCost().GetWithModifiers(CostModifiers::All) == 2,
            "monster-generated Frantic Escape incorrectly reduced Rocket Punch's cost");

    CardModel playerGeneratedStatus = f.combat.CreateCard(CardId::DAZED, f.player);
    CardPileCmd::AddGeneratedCardToCombat(playerGeneratedStatus, PileType::Discard, f.player);
    require(rocketPunch.EnergyCost().GetWithModifiers(CostModifiers::All) == 1,
            "a player-generated Status did not reduce Rocket Punch's cost");
}

}  // namespace

int main() {
    try {
        test_insatiable_statuses_keep_player_owner_but_no_player_creator();
        std::cout << "status creator audit regression: passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "status creator audit regression: " << error.what() << '\n';
        return 1;
    }
}
