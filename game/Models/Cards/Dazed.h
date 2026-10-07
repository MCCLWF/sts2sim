// Models/Cards/Dazed.cs
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct Dazed : CardModel {
    SIM_MODEL(Dazed, CardModel)

    int32_t MaxUpgradeLevel() const { return 0; }
};

}  // namespace sts2
