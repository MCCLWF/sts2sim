// Models/Cards/Greed.cs
#pragma once
#include "game/Models/CardModel.h"
namespace sts2 {
struct Greed : CardModel {
    SIM_MODEL(Greed, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
    bool CanBeGeneratedByModifiers() const { return false; }
};
}
