#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct WellLaidPlans : CardModel {
    SIM_MODEL(WellLaidPlans, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::WELL_LAID_PLANS_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};

}  // namespace sts2
