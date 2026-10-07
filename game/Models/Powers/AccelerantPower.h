#pragma once

#include "game/Models/PowerModel.h"

namespace sts2 {

// PoisonPower reads this stack count to determine its number of triggers.
struct AccelerantPower : PowerModel {
    SIM_MODEL(AccelerantPower, PowerModel)
};

}  // namespace sts2
