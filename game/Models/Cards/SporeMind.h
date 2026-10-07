// Models/Cards/SporeMind.cs
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct SporeMind : CardModel {
    SIM_MODEL(SporeMind, CardModel)
    bool CanBeGeneratedByModifiers() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }
};

}  // namespace sts2
