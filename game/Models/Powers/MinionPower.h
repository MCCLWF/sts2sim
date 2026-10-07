// Models/Powers/MinionPower.cs
#pragma once

#include "game/Models/PowerModel.h"

namespace sts2 {

struct MinionPower : PowerModel {
    SIM_MODEL(MinionPower, PowerModel)

    bool ShouldPlayVfx() const { return false; }
    bool OwnerIsSecondaryEnemy() const { return true; }
    bool ShouldPowerBeRemovedAfterOwnerDeath() const { return false; }
    bool ShouldOwnerDeathTriggerFatal() const { return false; }
};

}  // namespace sts2
