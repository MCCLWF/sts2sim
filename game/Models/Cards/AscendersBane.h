// Models/Cards/AscendersBane.cs
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct AscendersBane : CardModel {
    SIM_MODEL(AscendersBane, CardModel)
    bool CanBeGeneratedByModifiers() const { return false; }
    bool CanBeGeneratedInCombat() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }
};

}  // namespace sts2
