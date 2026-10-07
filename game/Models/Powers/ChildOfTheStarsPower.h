#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct ChildOfTheStarsPower : PowerModel {
    SIM_MODEL(ChildOfTheStarsPower, PowerModel)
    void AfterStarsSpent(int32_t amount, Player spender) const {
        if (amount > 0 && spender == Owner().Player())
            CreatureCmd::GainBlock(Owner(), Dec(Amount() * amount), ValueProp::Unpowered, nullptr);
    }
};
}  // namespace sts2
