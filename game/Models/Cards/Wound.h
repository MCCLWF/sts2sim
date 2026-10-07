// Models/Cards/Wound.cs
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct Wound : CardModel {
    SIM_MODEL(Wound, CardModel)

    int32_t MaxUpgradeLevel() const { return 0; }
};

}  // namespace sts2
