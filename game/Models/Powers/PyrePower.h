#pragma once
#include "game/Models/PowerModel.h"
namespace sts2 {
struct PyrePower : PowerModel { SIM_MODEL(PyrePower, PowerModel)
Dec ModifyMaxEnergy(Player player, Dec amount) const { return player==Owner().Player()?amount+Dec(Amount()):amount; }
}; }
