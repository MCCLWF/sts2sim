// Models/Relics/SignetRing.cs (v0.111.0).
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct SignetRing : RelicModel {
    SIM_MODEL(SignetRing, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const { PlayerCmd::GainGold(DynamicVars().Gold().BaseValue(), Owner()); }
};

}  // namespace sts2
