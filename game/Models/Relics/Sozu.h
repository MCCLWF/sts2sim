// Models/Relics/Sozu.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct Sozu : RelicModel {
    SIM_MODEL(Sozu, RelicModel)

    bool ShouldProcurePotion(PotionModel, Player player) const { return player != Owner(); }

    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        return player == Owner() ? amount + DynamicVars().Energy().BaseValue() : amount;
    }
};

}  // namespace sts2
