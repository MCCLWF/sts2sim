#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct ClarityPower : PowerModel {
    SIM_MODEL(ClarityPower, PowerModel)

    Dec ModifyHandDraw(Player player, Dec count) const {
        return player == Owner().Player() ? count + Dec(1) : count;
    }

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (participants.contains(Owner())) PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
