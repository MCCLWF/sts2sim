// Models/Cards/Injury.cs
#pragma once
#include "game/Models/CardModel.h"
namespace sts2 {
struct Injury : CardModel {
    SIM_MODEL(Injury, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
};
}
