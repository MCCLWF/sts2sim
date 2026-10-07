#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct StarNextTurnPower : PowerModel {
    SIM_MODEL(StarNextTurnPower, PowerModel)
    void AfterEnergyReset(Player player) const {
        if (player != Owner().Player()) return;
        PlayerCmd::GainStars(Dec(Amount()), player);
        PowerCmd::Remove(*this);
    }
};
}  // namespace sts2
