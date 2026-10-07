#pragma once

#include "game/Commands/ForgeCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct FurnacePower : PowerModel {
    SIM_MODEL(FurnacePower, PowerModel)
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (participants.contains(Owner())) ForgeCmd::Forge(Dec(Amount()), Owner().Player(), *this);
    }
};
}  // namespace sts2
