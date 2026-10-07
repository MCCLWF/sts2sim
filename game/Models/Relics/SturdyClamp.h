// Models/Relics/SturdyClamp.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct SturdyClamp : RelicModel {
    SIM_MODEL(SturdyClamp, RelicModel)

    bool ShouldClearBlock(Creature creature) const { return creature != Owner().Creature(); }

    void AfterPreventingBlockClear(AbstractModel preventer, Creature creature) const {
        if (preventer != *this || creature != Owner().Creature()) return;
        int32_t block = creature.Block();
        if (block > 10) {
            PlayerChoiceContext choiceContext;
            CreatureCmd::LoseBlock(choiceContext, creature, Dec(block - 10), Creature{});
        }
    }
};

}  // namespace sts2
