// Models/Cards/Debris.cs
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {
struct Debris : CardModel {
    SIM_MODEL(Debris, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {}
};
}  // namespace sts2
