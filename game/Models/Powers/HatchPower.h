// Models/Powers/HatchPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct HatchPower : PowerModel {
    SIM_MODEL(HatchPower, PowerModel)

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
