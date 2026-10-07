#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct JackOfAllTrades : CardModel {
    SIM_MODEL(JackOfAllTrades, CardModel)

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        CardList cards = CardFactory::GetForCombat(Owner(), CardPoolId::COLORLESS_CARD_POOL,
                                                   DynamicVars().Cards().IntValue(), Owner().RunState().Rng().CombatCardGeneration(),
                                                   CardType::None, true, CardId::JACK_OF_ALL_TRADES);
        for (CardModel card : cards) CardPileCmd::AddGeneratedCardToCombat(card, PileType::Hand, Owner());
    }

};

}  // namespace sts2
