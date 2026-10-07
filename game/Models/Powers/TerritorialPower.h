// Models/Powers/TerritorialPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct TerritorialPower : PowerModel {
    SIM_MODEL(TerritorialPower, PowerModel)

    void AfterSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide side, const CreatureList& participants) const {
        if (participants.contains(Owner())) {
            PowerCmd::Apply(PowerId::STRENGTH_POWER, choiceContext, Owner(), Dec(Amount()), Owner(), CardModel{});
        }
    }
};

}  // namespace sts2
