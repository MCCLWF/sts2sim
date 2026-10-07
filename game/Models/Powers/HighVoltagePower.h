// Models/Powers/HighVoltagePower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct HighVoltagePower : PowerModel {
    SIM_MODEL(HighVoltagePower, PowerModel)

    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner()))
            PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(Amount()), Owner(), CardModel{});
    }
};

}  // namespace sts2
