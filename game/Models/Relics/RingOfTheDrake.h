// Models/Relics/RingOfTheDrake.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct RingOfTheDrake : RelicModel {
    SIM_MODEL(RingOfTheDrake, RelicModel)

    Dec ModifyHandDraw(Player player, Dec count) const {
        if (player != Owner() || Dec(Owner().PlayerCombatState().TurnNumber()) > DynamicVars()["Turns"].BaseValue())
            return count;
        return count + DynamicVars().Cards().BaseValue();
    }
};

}  // namespace sts2
