// Models/Relics/Circlet.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct Circlet : RelicModel {
    SIM_MODEL(Circlet, RelicModel)
    bool IsStackable() const { return true; }
};

}  // namespace sts2
