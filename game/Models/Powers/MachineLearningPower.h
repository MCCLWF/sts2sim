#pragma once
#include "game/Models/PowerModel.h"
namespace sts2 {
struct MachineLearningPower : PowerModel { SIM_MODEL(MachineLearningPower, PowerModel)
    Dec ModifyHandDraw(Player player, Dec count) const { return player == Owner().Player() ? count + Dec(Amount()) : count; }
};
}
