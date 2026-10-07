#pragma once

#include "game/Models/Powers/TemporaryStrengthPower.h"

namespace sts2 {

struct PiercingWailPower : TemporaryStrengthPower {
    SIM_MODEL(PiercingWailPower, TemporaryStrengthPower)
    bool IsPositive() const override { return false; }

    void BeforeApplied(Creature target, Dec amount, Creature applier, CardModel cardSource) const {
        TemporaryStrengthPower::BeforeApplied(target, amount, applier, cardSource);
    }
    void AfterPowerAmountChanged(PlayerChoiceContext& ctx, PowerModel power, Dec amount, Creature applier,
                                 CardModel cardSource) const {
        TemporaryStrengthPower::AfterPowerAmountChanged(ctx, power, amount, applier, cardSource);
    }
    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide side, const CreatureList& participants) const {
        TemporaryStrengthPower::AfterSideTurnEnd(ctx, side, participants);
    }
};

}  // namespace sts2
