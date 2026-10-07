// Models/Cards/Writhe.cs
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct Writhe : CardModel {
    SIM_MODEL(Writhe, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
};

}  // namespace sts2
