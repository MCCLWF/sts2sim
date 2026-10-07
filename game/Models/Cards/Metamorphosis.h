// Models/Cards/Metamorphosis.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Metamorphosis : CardModel {
    SIM_MODEL(Metamorphosis, CardModel)

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        CardList cards = CardFactory::GetForCombat(
            Owner(), sts2::data(Owner().Character()).card_pool, DynamicVars().Cards().IntValue(),
            Owner().RunState().Rng().CombatCardGeneration(), CardType::Attack);
        for (CardModel card : cards) {
            card.SetToFreeThisCombat();
            CardPileCmd::AddGeneratedCardToCombat(card, PileType::Draw, Owner(), CardPilePosition::Random);
        }
    }
};

}  // namespace sts2
