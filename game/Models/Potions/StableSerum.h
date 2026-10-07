#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct StableSerum : PotionModel {
    SIM_MODEL(StableSerum, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::RETAIN_HAND_POWER, ctx, target, DynamicVars().Repeat().BaseValue(), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
