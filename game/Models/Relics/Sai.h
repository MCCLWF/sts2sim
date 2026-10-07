// Models/Relics/Sai.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Sai : RelicModel {
    SIM_MODEL(Sai, RelicModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature()))
            CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr);
    }
};

}  // namespace sts2
