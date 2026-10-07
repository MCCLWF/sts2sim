// Models/Relics/RunicPyramid.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct RunicPyramid : RelicModel {
    SIM_MODEL(RunicPyramid, RelicModel)

    bool ShouldFlush(Player player) const { return player != Owner(); }
};

}  // namespace sts2
