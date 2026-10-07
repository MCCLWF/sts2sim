// Models/Powers/HardToKillPower.cs
#pragma once

#include "game/Models/PowerModel.h"

namespace sts2 {

struct HardToKillPower : PowerModel {
    SIM_MODEL(HardToKillPower, PowerModel)

    Dec ModifyDamageCap(Creature target, ValueProp, Creature, CardModel, const CardPlay*) const {
        return target == Owner() ? Dec(Amount()) : Dec::max_value();
    }

    void AfterModifyingDamageAmount(CardModel) const { Flash(); }
};

}  // namespace sts2
