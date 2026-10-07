#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct SynchronizePower : PowerModel { SIM_MODEL(SynchronizePower, PowerModel)
    void BeforeApplied(Creature target, Dec amount, Creature applier, CardModel source) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FOCUS_POWER, ctx, target, amount, applier, source, true);
    }
    void AfterPowerAmountChanged(PlayerChoiceContext& ctx, PowerModel power, Dec amount, Creature applier, CardModel source) const {
        if (power == *this && amount != Dec(Amount())) PowerCmd::Apply(PowerId::FOCUS_POWER, ctx, Owner(), amount, applier, source, true);
    }
    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        int32_t amount = Amount();
        PowerCmd::Remove(*this);
        PowerCmd::Apply(PowerId::FOCUS_POWER, ctx, Owner(), Dec(0) - Dec(amount), Owner(), CardModel{});
    }
};
}
