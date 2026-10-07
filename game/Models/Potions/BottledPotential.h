#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct BottledPotential : PotionModel {
    SIM_MODEL(BottledPotential, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target) return;
        Player player = target.Player();
        CardPileCmd::Add(player.PlayerCombatState().Hand().Cards(), PileType::Draw);
        CardPileCmd::Shuffle(ctx, player);
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), player);
    }
};

}  // namespace sts2
