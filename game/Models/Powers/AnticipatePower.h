#pragma once

#include "game/Models/Powers/TemporaryDexterityPower.h"

namespace sts2 {

struct AnticipatePower : TemporaryDexterityPower {
    SIM_MODEL(AnticipatePower, TemporaryDexterityPower)

    void BeforeApplied(Creature target, Dec amount, Creature applier, CardModel cardSource) const {
        TemporaryDexterityPower::BeforeApplied(target, amount, applier, cardSource);
    }
    void AfterPowerAmountChanged(PlayerChoiceContext& ctx, PowerModel power, Dec amount, Creature applier,
                                 CardModel cardSource) const {
        TemporaryDexterityPower::AfterPowerAmountChanged(ctx, power, amount, applier, cardSource);
    }
    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide side, const CreatureList& participants) const {
        TemporaryDexterityPower::AfterSideTurnEnd(ctx, side, participants);
    }
};

}  // namespace sts2
