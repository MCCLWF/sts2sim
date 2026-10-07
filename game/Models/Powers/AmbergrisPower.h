// Models/Powers/AmbergrisPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct AmbergrisPower : PowerModel {
    SIM_MODEL(AmbergrisPower, PowerModel)

    bool ShouldTakeExtraTurn(Player player) const {
        return Amount() > 0 && player == Owner().Player();
    }

    void AfterTakingExtraTurn(Player player) const {
        if (player == Owner().Player()) PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
