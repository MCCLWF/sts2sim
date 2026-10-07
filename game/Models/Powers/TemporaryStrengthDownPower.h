#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
// TemporaryStrengthPower<T> where IsPositive == false: apply negative Strength,
// mirror subsequent amount changes, then restore the delta when its owner ends
// a side turn.
struct TemporaryStrengthDownPower : PowerModel {
    using PowerModel::PowerModel;
    void BeforeApplied(Creature target, Dec amount, Creature applier, CardModel cardSource) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, target, Dec(0) - amount, applier, cardSource, true);
    }
    void AfterPowerAmountChanged(PlayerChoiceContext& ctx, PowerModel power, Dec amount, Creature applier, CardModel cardSource) const {
        if (power == *this && amount != Dec(Amount()))
            PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(0) - amount, applier, cardSource, true);
    }
    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        int32_t amount = Amount();
        PowerCmd::Remove(*this);
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(amount), Owner(), CardModel{});
    }
};
}  // namespace sts2
