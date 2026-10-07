#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct RadiancePower : PowerModel {
    SIM_MODEL(RadiancePower, PowerModel)

    void AfterEnergyReset(Player player) const {
        if (player != Owner().Player()) return;
        PlayerCmd::GainEnergy(DynamicVars().Energy().IntValue(), player);
        PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
