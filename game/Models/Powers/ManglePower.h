#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

// TemporaryStrengthPower<Mangle>: the card applies a negative Strength delta
// for the lifetime of this power, then restores it when the side turn ends.
struct ManglePower : PowerModel {
    SIM_MODEL(ManglePower, PowerModel)

    void BeforeApplied(Creature target, Dec amount, Creature applier, CardModel cardSource) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, target, Dec(0) - amount, applier, cardSource, true);
    }

    void AfterPowerAmountChanged(PlayerChoiceContext& ctx, PowerModel power, Dec amount, Creature applier,
                                 CardModel cardSource) const {
        if (power != *this || amount == Dec(0) || amount == Dec(Amount())) return;
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(0) - amount, applier, cardSource, true);
    }

    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        PowerCmd::Remove(*this);
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(Amount()), Owner(), CardModel{});
    }
};

}  // namespace sts2
