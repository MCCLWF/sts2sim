// Models/Relics/TheAbacus.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct TheAbacus : RelicModel {
    SIM_MODEL(TheAbacus, RelicModel)

    void AfterShuffle(PlayerChoiceContext&, Player shuffler) const {
        if (shuffler == Owner())
            CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr);
    }
};

}  // namespace sts2
