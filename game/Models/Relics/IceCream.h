// Models/Relics/IceCream.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct IceCream : RelicModel {
    SIM_MODEL(IceCream, RelicModel)
    bool ShouldPlayerResetEnergy(Player player) const {
        return player != Owner() || Owner().PlayerCombatState().TurnNumber() == 1;
    }
};

}  // namespace sts2
