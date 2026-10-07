// Models/Powers/ThieveryPower.cs
#pragma once

#include <algorithm>

#include "game/Commands/PlayerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct ThieveryPower : PowerModel {
    SIM_MODEL(ThieveryPower, PowerModel)

    void Steal() const {
        Creature target = Target();
        if (!target || target.IsDead() || target.Player().Gold() <= 0) return;
        int32_t amount = std::min(Amount(), target.Player().Gold());
        PlayerCmd::LoseGold(Dec(amount), target.Player(), GoldLossType::Stolen);
        DynamicVars().Gold().UpgradeValueBy(Dec(amount));
    }
};

}  // namespace sts2
