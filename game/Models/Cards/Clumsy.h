// Models/Cards/Clumsy.cs
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct Clumsy : CardModel {
    SIM_MODEL(Clumsy, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
};

}  // namespace sts2
