#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct LuckyTonic : PotionModel {
    SIM_MODEL(LuckyTonic, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::BUFFER_POWER, ctx, target, DynamicVars()["BufferPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
