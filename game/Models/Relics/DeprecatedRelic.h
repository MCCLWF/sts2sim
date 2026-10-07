// Models/Relics/DeprecatedRelic.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct DeprecatedRelic : RelicModel {
    SIM_MODEL(DeprecatedRelic, RelicModel)
    bool IsStackable() const { return true; }
};

}  // namespace sts2
