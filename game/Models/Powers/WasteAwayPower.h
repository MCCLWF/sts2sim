// Models/Powers/WasteAwayPower.cs
#pragma once
#include "game/Models/PowerModel.h"
namespace sts2 {
struct WasteAwayPower : PowerModel {
    SIM_MODEL(WasteAwayPower, PowerModel)
    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        return player == Owner().Player() ? amount - Dec(Amount()) : amount;
    }
};
}  // namespace sts2
