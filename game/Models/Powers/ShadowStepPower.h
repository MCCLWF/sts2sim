#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct ShadowStepPower : PowerModel {
    SIM_MODEL(ShadowStepPower, PowerModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (!participants.contains(Owner())) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::DOUBLE_DAMAGE_POWER, ctx, Owner(), Dec(Amount()), Owner(), CardModel{});
        PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
