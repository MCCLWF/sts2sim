#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {
struct MiniRegent : RelicModel {
    SIM_MODEL(MiniRegent, RelicModel)
    void AfterStarsSpent(int32_t, Player spender) const {
        if (spender != Owner() || extra(0)) return;
        extra(0) = 1;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner().Creature(), DynamicVars().Strength().BaseValue(), Owner().Creature(), CardModel{});
    }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (participants.contains(Owner().Creature())) extra(0) = 0;
    }
    void AfterCombatEnd(CombatRoom) const { extra(0) = 0; }
};
}  // namespace sts2
