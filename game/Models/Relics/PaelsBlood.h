// Models/Relics/PaelsBlood.cs (v0.111.0).
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct PaelsBlood : RelicModel {
    SIM_MODEL(PaelsBlood, RelicModel)
    Dec ModifyHandDraw(Player player, Dec count) const {
        return player == Owner() ? count + DynamicVars().Cards().BaseValue() : count;
    }
};

}  // namespace sts2
