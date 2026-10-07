// Models/Cards/PoorSleep.cs
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct PoorSleep : CardModel {
    SIM_MODEL(PoorSleep, CardModel)
    bool CanBeGeneratedByModifiers() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }
};

}  // namespace sts2
