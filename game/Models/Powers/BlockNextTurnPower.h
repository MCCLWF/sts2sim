// Models/Powers/BlockNextTurnPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct BlockNextTurnPower : PowerModel {
    SIM_MODEL(BlockNextTurnPower, PowerModel)
    void AfterBlockCleared(Creature creature) const {
        if (creature != Owner()) return;
        CreatureCmd::GainBlock(Owner(), Dec(Amount()), ValueProp::Unpowered, nullptr);
        PowerCmd::Remove(*this);
    }
};
}
