// Models/Powers/HyperbeamFocusDownPower.cs and TemporaryFocusPower.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct HyperbeamFocusDownPower : PowerModel { SIM_MODEL(HyperbeamFocusDownPower, PowerModel)
    void BeforeApplied(Creature target, Dec amount, Creature applier, CardModel cardSource) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FOCUS_POWER, ctx, target, Dec(0) - amount, applier, cardSource, true);
    }
    void AfterPowerAmountChanged(PlayerChoiceContext& ctx, PowerModel power, Dec amount, Creature applier,
                                 CardModel cardSource) const {
        if (power == *this && amount != Dec(Amount()))
            PowerCmd::Apply(PowerId::FOCUS_POWER, ctx, Owner(), Dec(0) - amount, applier, cardSource, true);
    }
    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        PowerCmd::Remove(*this);
        PowerCmd::Apply(PowerId::FOCUS_POWER, ctx, Owner(), Dec(Amount()), Owner(), CardModel{});
    }
};
}
