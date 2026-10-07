// Models/Powers/ReptileTrinketPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct ReptileTrinketPower : PowerModel {
    SIM_MODEL(ReptileTrinketPower, PowerModel)

    void BeforeApplied(Creature target, Dec amount, Creature applier, CardModel cardSource) const {
        PlayerChoiceContext choiceContext = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, choiceContext, target, amount, applier, cardSource, true);
    }

    void AfterPowerAmountChanged(PlayerChoiceContext& choiceContext, PowerModel power, Dec amount,
                                 Creature applier, CardModel cardSource) const {
        if (power != *this || amount == Dec(Amount())) return;
        PowerCmd::Apply(PowerId::STRENGTH_POWER, choiceContext, Owner(), amount, applier, cardSource, true);
    }

    void AfterSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        int32_t amount = Amount();
        PowerCmd::Remove(*this);
        PowerCmd::Apply(PowerId::STRENGTH_POWER, choiceContext, Owner(), Dec(0 - amount), Owner(), CardModel{});
    }
};

}  // namespace sts2
