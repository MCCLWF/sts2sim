#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct WraithFormPower : PowerModel {
    SIM_MODEL(WraithFormPower, PowerModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (!participants.contains(Owner())) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, Owner(), Dec(0) - Dec(Amount()), Owner(), CardModel{});
    }
};

}  // namespace sts2
