// Models/Relics/PetrifiedToad.cs
#pragma once

#include "game/Commands/PotionCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PetrifiedToad : RelicModel {
    SIM_MODEL(PetrifiedToad, RelicModel)

    void BeforeCombatStartLate() const {
        PotionCmd::TryToProcure(PotionId::POTION_SHAPED_ROCK, Owner());
    }
};

}  // namespace sts2
