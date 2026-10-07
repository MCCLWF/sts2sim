#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct PotionOfDoom : PotionModel {
    SIM_MODEL(PotionOfDoom, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target) throw GameError("Potion target is null");
        PowerCmd::Apply(PowerId::DOOM_POWER, ctx, target, DynamicVars().Doom().BaseValue(), Owner().Creature(), {});
    }
};

}  // namespace sts2
