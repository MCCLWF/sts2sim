// Models/Relics/RingingTriangle.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct RingingTriangle : RelicModel {
    SIM_MODEL(RingingTriangle, RelicModel)

    bool ShouldFlush(Player player) const {
        if (player != Owner()) return true;
        return Owner().PlayerCombatState().TurnNumber() > 1;
    }
};

}  // namespace sts2
