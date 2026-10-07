#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct PowerPotion : PotionModel {
    SIM_MODEL(PowerPotion, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        Player player = target.Player();
        CardList cards = CardFactory::GetForCombat(player, sts2::data(player.Character()).card_pool, 3,
                                                   player.RunState().Rng().CombatCardGeneration(), CardType::Power, true);
        CardModel chosen = CardSelectCmd::FromChooseACardScreen(ctx, cards, player, true);
        if (chosen) {
            chosen.SetToFreeThisTurn();
            CardPileCmd::AddGeneratedCardToCombat(chosen, PileType::Hand, Owner());
        }
    }
};

}  // namespace sts2
