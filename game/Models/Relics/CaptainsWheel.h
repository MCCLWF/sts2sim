// Models/Relics/CaptainsWheel.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct CaptainsWheel : RelicModel {
    SIM_MODEL(CaptainsWheel, RelicModel)
    void AfterBlockCleared(Creature creature) const {
        if (creature == Owner().Creature() && Owner().PlayerCombatState().TurnNumber() == 3)
            CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr);
    }
};

}  // namespace sts2
