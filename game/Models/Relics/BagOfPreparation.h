#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct BagOfPreparation : RelicModel {
    SIM_MODEL(BagOfPreparation, RelicModel)

    Dec ModifyHandDraw(Player player, Dec count) const {
        if (player != Owner() || !Owner().PlayerCombatState() || Owner().PlayerCombatState().TurnNumber() > 1) return count;
        return count + DynamicVars().Cards().BaseValue();
    }
};

}  // namespace sts2
