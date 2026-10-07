// Models/Cards/Disintegration.cs
#pragma once
#include "game/Models/CardModel.h"
namespace sts2 {
struct Disintegration : CardModel {
    SIM_MODEL(Disintegration, CardModel)
    bool CanBeGeneratedInCombat() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }
};
}  // namespace sts2
