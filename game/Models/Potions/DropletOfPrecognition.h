#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct DropletOfPrecognition : PotionModel {
    SIM_MODEL(DropletOfPrecognition, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target) return;
        Player player = target.Player();
        CardModel card = CardSelectCmd::FromCombatPile(ctx, player.PlayerCombatState().DrawPile(), player,
                                                        CardSelectorPrefs(1)).front();
        if (card) CardPileCmd::Add(card, PileType::Hand);
    }
};

}  // namespace sts2
