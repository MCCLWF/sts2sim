// Models/Relics/GoldenPearl.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct GoldenPearl : RelicModel {
    SIM_MODEL(GoldenPearl, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { PlayerCmd::GainGold(Dec(150), Owner()); }
};
}
