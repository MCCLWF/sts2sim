#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct TemporaryStrengthPower : PowerModel {
    SIM_MODEL(TemporaryStrengthPower, PowerModel)

    int32_t Sign() const { return IsPositive() ? 1 : -1; }
    virtual bool IsPositive() const { return true; }

    void BeforeApplied(Creature target, Dec amount, Creature applier, CardModel cardSource) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, target, Dec(Sign()) * amount, applier, cardSource, true);
    }

    void AfterPowerAmountChanged(PlayerChoiceContext& ctx, PowerModel power, Dec amount, Creature applier,
                                 CardModel cardSource) const {
        if (power != *this || amount == Dec(Amount())) return;
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(Sign()) * amount, applier, cardSource, true);
    }

    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        int32_t remaining = Amount();
        PowerCmd::Remove(*this);
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(-Sign() * remaining), Owner(), CardModel{});
    }
};

}  // namespace sts2
