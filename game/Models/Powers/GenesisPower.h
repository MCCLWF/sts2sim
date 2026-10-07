#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct GenesisPower : PowerModel {
    SIM_MODEL(GenesisPower, PowerModel)
    void AfterEnergyReset(Player player) const { if (player == Owner().Player()) PlayerCmd::GainStars(Dec(Amount()), player); }
};
}  // namespace sts2
