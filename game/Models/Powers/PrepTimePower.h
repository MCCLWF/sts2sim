#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct PrepTimePower : PowerModel {
    SIM_MODEL(PrepTimePower, PowerModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (!participants.contains(Owner())) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::VIGOR_POWER, ctx, Owner(), Dec(Amount()), Owner(), CardModel{});
    }
};

}  // namespace sts2
