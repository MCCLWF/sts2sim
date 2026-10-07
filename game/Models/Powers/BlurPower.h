#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct BlurPower : PowerModel {
    SIM_MODEL(BlurPower, PowerModel)

    bool ShouldClearBlock(Creature creature) const { return creature != Owner(); }

    void AfterPreventingBlockClear(AbstractModel, Creature) const {
        // Flash() in the source is presentation only.
    }

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (participants.contains(Owner())) PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
