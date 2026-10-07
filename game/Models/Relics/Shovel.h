// Models/Relics/Shovel.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Shovel : RelicModel {
    SIM_MODEL(Shovel, RelicModel)
    // TryModifyRestSiteOptions is implemented in RestSiteOption::Generate.
    bool IsAllowed(RunState runState) const {
        return IsBeforeAct3TreasureChest(runState);
    }
};

}  // namespace sts2
