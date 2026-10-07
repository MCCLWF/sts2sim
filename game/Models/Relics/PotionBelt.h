// Models/Relics/PotionBelt.cs
#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct PotionBelt : RelicModel {
    SIM_MODEL(PotionBelt, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { PlayerCmd::GainMaxPotionCount(DynamicVars()["PotionSlots"].IntValue(), Owner()); }
};
}
