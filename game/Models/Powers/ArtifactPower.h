// Models/Powers/ArtifactPower.cs
#pragma once

#include "game/Combat/CombatState.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct ArtifactPower : PowerModel {
    SIM_MODEL(ArtifactPower, PowerModel)

    bool TryModifyPowerAmountReceived(PowerModel canonicalPower, Creature target, Dec amount, Creature applier,
                                      Dec& modifiedAmount) const {
        if (target != Owner()) {
            modifiedAmount = amount;
            return false;
        }
        if (canonicalPower.GetTypeForAmount(amount) != PowerType::Debuff) {
            modifiedAmount = amount;
            return false;
        }
        if (!canonicalPower.IsVisible()) {
            modifiedAmount = amount;
            return false;
        }
        modifiedAmount = Dec(0);
        return true;
    }

    void AfterModifyingPowerAmountReceived(PowerModel power) const { PowerCmd::Decrement(*this); }

    Dec GetScaledAmountForMultiplayer(sts2::CombatState combatState, Creature applier, Dec amount, Creature target,
                                      CardModel cardSource) const {
        return amount + Dec(static_cast<int32_t>(combatState.Players().size())) - Dec(1);
    }
};

}  // namespace sts2
