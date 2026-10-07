#pragma once

#include "game/Models/PowerModel.h"

namespace sts2 {

struct WellLaidPlansPower : PowerModel {
    SIM_MODEL(WellLaidPlansPower, PowerModel)

    bool ShouldFlush(Player player) const { return player != Owner().Player(); }
};

}  // namespace sts2
