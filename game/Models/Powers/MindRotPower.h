// Models/Powers/MindRotPower.cs
#pragma once
#include <algorithm>
#include "game/Models/PowerModel.h"
namespace sts2 {
struct MindRotPower : PowerModel {
    SIM_MODEL(MindRotPower, PowerModel)
    Dec ModifyHandDraw(Player player, Dec count) const {
        return player == Owner().Player() ? std::max(Dec(0), count - Dec(Amount())) : count;
    }
    void AfterModifyingHandDraw() const {}
};
}  // namespace sts2
