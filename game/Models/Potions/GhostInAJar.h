#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct GhostInAJar : PotionModel {
    SIM_MODEL(GhostInAJar, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target || !target.Player()) throw GameError("GhostInAJar: target is not a player");
        PowerCmd::Apply(PowerId::INTANGIBLE_POWER, ctx, target, DynamicVars()["IntangiblePower"].BaseValue(),
                        Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
