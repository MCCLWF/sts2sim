// Models/Cards/CurseOfTheBell.cs
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct CurseOfTheBell : CardModel {
    SIM_MODEL(CurseOfTheBell, CardModel)
    bool CanBeGeneratedByModifiers() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }
};

}  // namespace sts2
