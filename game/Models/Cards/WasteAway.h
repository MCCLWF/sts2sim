// Models/Cards/WasteAway.cs
#pragma once
#include "game/Models/CardModel.h"
namespace sts2 {
struct WasteAway : CardModel {
    SIM_MODEL(WasteAway, CardModel)
    bool CanBeGeneratedInCombat() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }
};
}  // namespace sts2
