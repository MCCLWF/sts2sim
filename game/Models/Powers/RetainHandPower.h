#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct RetainHandPower : PowerModel {
    SIM_MODEL(RetainHandPower, PowerModel)

    bool ShouldFlush(Player player) const { return player != Owner().Player(); }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
