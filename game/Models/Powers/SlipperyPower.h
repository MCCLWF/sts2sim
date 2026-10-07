// Models/Powers/SlipperyPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct SlipperyPower : PowerModel {
    SIM_MODEL(SlipperyPower, PowerModel)

    Dec ModifyHpLostAfterOsty(Creature target, Dec amount, ValueProp props, Creature dealer, CardModel cardSource) const {
        if (target != Owner() || amount < Dec(1)) return amount;
        return Dec(1);
    }

    void AfterDamageReceived(PlayerChoiceContext& choiceContext, Creature target, const DamageResult& result,
                             ValueProp props, Creature dealer, CardModel cardSource) const {
        if (target == Owner() && result.UnblockedDamage >= 1) PowerCmd::Decrement(*this);
    }

    Dec GetScaledAmountForMultiplayer(sts2::CombatState combatState, Creature applier, Dec amount, Creature target,
                                      CardModel cardSource) const {
        return amount * Dec(static_cast<int32_t>(combatState.Players().size()));
    }
};

}  // namespace sts2
