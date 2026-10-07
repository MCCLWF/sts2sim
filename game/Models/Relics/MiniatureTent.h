// Models/Relics/MiniatureTent.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct MiniatureTent : RelicModel {
    SIM_MODEL(MiniatureTent, RelicModel)

    bool ShouldDisableRemainingRestSiteOptions(Player player) const {
        if (player != Owner()) return true;
        Flash();
        return false;
    }
};

}  // namespace sts2
