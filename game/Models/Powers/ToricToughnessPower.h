// Models/Powers/ToricToughnessPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct ToricToughnessPower : PowerModel {
    SIM_MODEL(ToricToughnessPower, PowerModel)

    // The power amount tracks the remaining turns.  Its Block dynamic var is
    // the separate amount to grant whenever the owner's block is cleared.
    void SetBlock(Dec block) const { DynamicVars().Block().SetBaseValue(block); }

    void AfterBlockCleared(Creature creature) const {
        if (creature != Owner()) return;
        Flash();
        CreatureCmd::GainBlock(Owner(), DynamicVars().Block(), nullptr);
        PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
