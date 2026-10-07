#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct WhiteNoise : CardModel { SIM_MODEL(WhiteNoise, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        Player player = Owner();
        CardList cards = CardFactory::GetForCombat(player, sts2::data(player.Character()).card_pool, 1,
            player.RunState().Rng().CombatCardGeneration(), CardType::Power, true);
        if (cards.empty()) return;
        cards[0].SetToFreeThisTurn();
        CardPileCmd::AddGeneratedCardToCombat(cards[0], PileType::Hand, player);
    }
};
}
