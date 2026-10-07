#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct Duplicator : PotionModel {
    SIM_MODEL(Duplicator, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::DUPLICATION_POWER, ctx, target, Dec(1), target, CardModel{});
    }
};

}  // namespace sts2
