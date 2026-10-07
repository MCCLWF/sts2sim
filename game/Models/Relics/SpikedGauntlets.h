// Models/Relics/SpikedGauntlets.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct SpikedGauntlets : RelicModel {
    SIM_MODEL(SpikedGauntlets, RelicModel)

    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        return player == Owner() ? amount + DynamicVars().Energy().BaseValue() : amount;
    }
    bool TryModifyEnergyCostInCombat(CardModel card, Dec originalCost, Dec& modifiedCost) const {
        modifiedCost = originalCost;
        if (card.Owner().Creature() != Owner().Creature() || card.Type() != CardType::Power) return false;
        modifiedCost = originalCost + Dec(1);
        return true;
    }
};

}  // namespace sts2
