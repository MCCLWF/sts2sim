#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct PowderedDemise : PotionModel {
    SIM_MODEL(PowderedDemise, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::DEMISE_POWER, ctx, target, DynamicVars()["Demise"].BaseValue(), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
