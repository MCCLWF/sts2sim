#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Abundance : CardModel {
    SIM_MODEL(Abundance, CardModel)

    bool CanBeGeneratedInCombat() const { return false; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList cards = CardFactory::GetForCombat(Owner(), sts2::data(Owner().Character()).card_pool, 3,
                                                   Owner().RunState().Rng().CombatCardGeneration(), CardType::Power, true);
        for (CardModel card : cards) CardCmd::Upgrade(card);
        CardModel chosen = CardSelectCmd::FromChooseACardScreen(ctx, cards, Owner());
        if (chosen) {
            chosen.SetToFreeThisTurn();
            CardPileCmd::AddGeneratedCardToCombat(chosen, PileType::Hand, Owner());
        }
    }

};

}  // namespace sts2
