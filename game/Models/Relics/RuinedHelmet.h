// Models/Relics/RuinedHelmet.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct RuinedHelmet : RelicModel {
    SIM_MODEL(RuinedHelmet, RelicModel)

    bool TryModifyPowerAmountReceived(PowerModel canonicalPower, Creature target, Dec amount,
                                      Creature, Dec& modifiedAmount) const {
        modifiedAmount = amount;
        if (canonicalPower.PowerId_() != PowerId::STRENGTH_POWER ||
            target != Owner().Creature() || amount <= Dec(0) || extra(0)) return false;
        modifiedAmount *= Dec(2);
        return true;
    }

    void AfterModifyingPowerAmountReceived(PowerModel) const { extra(0) = 1; }
    void AfterCombatEnd(CombatRoom) const { extra(0) = 0; }
};

}  // namespace sts2
