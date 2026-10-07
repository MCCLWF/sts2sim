// Models/Relics/HornCleat.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct HornCleat : RelicModel {
    SIM_MODEL(HornCleat, RelicModel)

    void AfterBlockCleared(Creature creature) const {
        if (creature == Owner().Creature() && Owner().PlayerCombatState().TurnNumber() == 2)
            CreatureCmd::GainBlock(Owner().Creature(), DynamicVars()["Block"], nullptr);
    }
};

}  // namespace sts2
