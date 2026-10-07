// Models/Cards/MindRot.cs
#pragma once
#include "game/Models/CardModel.h"
namespace sts2 {
struct MindRot : CardModel {
    SIM_MODEL(MindRot, CardModel)
    bool CanBeGeneratedInCombat() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }
};
}  // namespace sts2
