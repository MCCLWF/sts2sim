// Models/Powers/StrengthPower.cs
#pragma once

#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct StrengthPower : PowerModel {
    SIM_MODEL(StrengthPower, PowerModel)

    Dec ModifyDamageAdditive(Creature target, Dec amount, ValueProp props, Creature dealer, CardModel cardSource,
                             const CardPlay* cardPlay) const {
        if (Owner() != dealer) return Dec(0);
        if (!IsPoweredAttack(props)) return Dec(0);
        return Dec(Amount());
    }
};

}  // namespace sts2
