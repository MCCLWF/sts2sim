#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct MasterPlanner : CardModel {
    SIM_MODEL(MasterPlanner, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::MASTER_PLANNER_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};

}  // namespace sts2
