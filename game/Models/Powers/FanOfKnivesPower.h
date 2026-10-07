#pragma once

#include "game/Models/PowerModel.h"

namespace sts2 {

// Shiv checks for this single-instance power to change from single-target to
// all-enemy targeting.
struct FanOfKnivesPower : PowerModel {
    SIM_MODEL(FanOfKnivesPower, PowerModel)
};

}  // namespace sts2
