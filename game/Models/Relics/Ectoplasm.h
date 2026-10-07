// Models/Relics/Ectoplasm.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct Ectoplasm : RelicModel {
    SIM_MODEL(Ectoplasm, RelicModel)

    Dec ModifyGoldGained(Player player, Dec amount) const { return player == Owner() ? Dec(0) : amount; }
    void AfterModifyingGoldGained(Player, Dec) const { Flash(); }

    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        return player == Owner() ? amount + DynamicVars().Energy().BaseValue() : amount;
    }
};

}  // namespace sts2
