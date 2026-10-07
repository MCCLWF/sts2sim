// Models/Powers/EnergyNextTurnPower.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct EnergyNextTurnPower : PowerModel {
    SIM_MODEL(EnergyNextTurnPower, PowerModel)

    void AfterEnergyReset(Player player) const {
        if (player != Owner().Player()) return;
        PlayerCmd::GainEnergy(Dec(Amount()), player);
        PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
