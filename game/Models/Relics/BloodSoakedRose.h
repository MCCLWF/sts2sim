// Models/Relics/BloodSoakedRose.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BloodSoakedRose : RelicModel {
    SIM_MODEL(BloodSoakedRose, RelicModel)

    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { CardPileCmd::AddCurseToDeck(CardId::ENTHRALLED, Owner()); }
    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        return player == Owner() ? amount + DynamicVars().Energy().BaseValue() : amount;
    }
};

}  // namespace sts2
