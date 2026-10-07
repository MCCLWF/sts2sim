#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct SneckoSkull : RelicModel {
    SIM_MODEL(SneckoSkull, RelicModel)

    Dec ModifyPowerAmountGivenAdditive(PowerModel power, Creature giver, Dec, Creature, CardModel) const {
        if (power.PowerId_() != PowerId::POISON_POWER || giver != Owner().Creature()) return Dec(0);
        return DynamicVars().Poison().BaseValue();
    }

    void AfterModifyingPowerAmountGiven(PowerModel) const { Flash(); }
};

}  // namespace sts2
