#pragma once

#include "game/Models/Powers/TemporaryStrengthDownPower.h"

namespace sts2 {
struct MonarchsGazeStrengthDownPower : TemporaryStrengthDownPower {
    SIM_MODEL(MonarchsGazeStrengthDownPower, TemporaryStrengthDownPower)
    void BeforeApplied(Creature target, Dec amount, Creature applier, CardModel cardSource) const {
        TemporaryStrengthDownPower::BeforeApplied(target, amount, applier, cardSource);
    }
    void AfterPowerAmountChanged(PlayerChoiceContext& ctx, PowerModel power, Dec amount, Creature applier, CardModel cardSource) const {
        TemporaryStrengthDownPower::AfterPowerAmountChanged(ctx, power, amount, applier, cardSource);
    }
    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide side, const CreatureList& participants) const {
        TemporaryStrengthDownPower::AfterSideTurnEnd(ctx, side, participants);
    }
};
}  // namespace sts2
