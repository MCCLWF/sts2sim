#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct ColorlessPotion : PotionModel {
    SIM_MODEL(ColorlessPotion, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target || !target.IsPlayer()) throw GameError("ColorlessPotion: invalid target");
        Player player = target.Player();
        CardList cards = CardFactory::GetForCombat(player, CardPoolId::COLORLESS_CARD_POOL, 3,
                                                   player.RunState().Rng().CombatCardGeneration(), CardType::None, true);
        CardModel chosen = CardSelectCmd::FromChooseACardScreen(ctx, cards, player, true);
        if (chosen) {
            chosen.SetToFreeThisTurn();
            CardPileCmd::AddGeneratedCardToCombat(chosen, PileType::Hand, Owner());
        }
    }
};

}  // namespace sts2
