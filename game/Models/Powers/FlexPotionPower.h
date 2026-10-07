#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct FlexPotionPower : PowerModel {
    SIM_MODEL(FlexPotionPower, PowerModel)

    void BeforeApplied(Creature target, Dec amount, Creature applier, CardModel cardSource) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, target, amount, applier, cardSource, true);
    }

    void AfterPowerAmountChanged(PlayerChoiceContext& ctx, PowerModel power, Dec amount, Creature applier,
                                 CardModel cardSource) const {
        if (power != *this || amount == Dec(Amount())) return;
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), amount, applier, cardSource, true);
    }

    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        int amount = Amount();
        PowerCmd::Remove(*this);
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(0 - amount), Owner(), CardModel{});
    }
};

}  // namespace sts2
