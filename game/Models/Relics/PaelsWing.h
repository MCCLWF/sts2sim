// Models/Relics/PaelsWing.cs (v0.111.0).
#pragma once

#include "game/Commands/RelicCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PaelsWing : RelicModel {
    SIM_MODEL(PaelsWing, RelicModel)

    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return extra(0) % DynamicVars()["Sacrifices"].IntValue(); }

    void OnSacrifice() const {
        ++extra(0);  // RewardsSacrificed, the original SavedProperty.
        Flash();
        if (extra(0) % DynamicVars()["Sacrifices"].IntValue() == 0)
            RelicCmd::Obtain(RelicFactory::PullNextRelicFromFront(Owner()), Owner());
    }
};

}  // namespace sts2
