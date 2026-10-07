#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct PillarOfCreationPower : PowerModel {
    SIM_MODEL(PillarOfCreationPower, PowerModel)
    void AfterCardGeneratedForCombat(CardModel, Player creator) const {
        if (creator && creator.Creature() == Owner())
            CreatureCmd::GainBlock(Owner(), Dec(Amount()), ValueProp::Unpowered, nullptr);
    }
};
}  // namespace sts2
