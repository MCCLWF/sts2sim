#pragma once

#include "game/Combat/CombatState.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct MonologuePower : PowerModel {
    SIM_MODEL(MonologuePower, PowerModel)
    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || play->Card.Owner().Creature() != Owner()) return;
        CombatState().CaptureCardPlayPowerAmount(*this, play->Card, DynamicVars().Strength().IntValue());
    }
    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner().Player()) return;
        int32_t strength = 0;
        if (!CombatState().TakeCardPlayPowerAmount(*this, play->Card, strength)) return;
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(strength), Owner(), CardModel{}, true);
        DynamicVars()["StrengthApplied"].SetBaseValue(
            DynamicVars()["StrengthApplied"].BaseValue() + Dec(DynamicVars().Strength().IntValue()));
    }
    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        int32_t strengthApplied = DynamicVars()["StrengthApplied"].IntValue();
        CombatState().ForgetCardPlayPowerCaptures(*this);
        PowerCmd::Remove(*this);
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(0) - strengthApplied, Owner(), CardModel{}, true);
    }
};
}  // namespace sts2
