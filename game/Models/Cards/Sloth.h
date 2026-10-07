// Models/Cards/Sloth.cs
#pragma once
#include "game/Models/CardModel.h"
namespace sts2 {
struct Sloth : CardModel {
    SIM_MODEL(Sloth, CardModel)
    bool CanBeGeneratedInCombat() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }
};
}  // namespace sts2
