#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Distraction : CardModel {
    SIM_MODEL(Distraction, CardModel)

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        CardList cards = CardFactory::GetForCombat(Owner(), sts2::data(Owner().Character()).card_pool, 1,
                                                   Owner().RunState().Rng().CombatCardGeneration(), CardType::Skill, true);
        if (!cards.empty()) {
            cards[0].SetToFreeThisTurn();
            CardPileCmd::AddGeneratedCardToCombat(cards[0], PileType::Hand, Owner());
        }
    }

};

}  // namespace sts2
