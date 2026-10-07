// Models/Cards/Folly.cs
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct Folly : CardModel {
    SIM_MODEL(Folly, CardModel)
    bool CanBeGeneratedByModifiers() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }
};

}  // namespace sts2
