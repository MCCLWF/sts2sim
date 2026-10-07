#pragma once

#include "game/Models/Powers/TemporaryStrengthPower.h"

namespace sts2 {

// The source model inherits TemporaryStrengthPower: Feeding Frenzy raises
// Strength while active, mirrors stack changes, and removes that Strength
// when its owner's turn ends.
struct FeedingFrenzyPower : TemporaryStrengthPower {
    SIM_MODEL(FeedingFrenzyPower, TemporaryStrengthPower)

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
