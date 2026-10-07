// Models/Cards/Soot.cs (Slay the Spire 2 v0.111.0)
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct Soot : CardModel {
    SIM_MODEL(Soot, CardModel)

    bool CanBeGeneratedInCombat() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }
};

}  // namespace sts2
