// Models/Relics/RingOfTheSnake.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct RingOfTheSnake : RelicModel {
    SIM_MODEL(RingOfTheSnake, RelicModel)

    Dec ModifyHandDraw(Player player, Dec count) const {
        if (player != Owner() || Owner().PlayerCombatState().TurnNumber() > 1) return count;
        return count + DynamicVars().Cards().BaseValue();
    }
};

}  // namespace sts2
