#pragma once

#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct SoarPower : PowerModel {
    SIM_MODEL(SoarPower, PowerModel)

    Dec ModifyDamageMultiplicative(Creature target, Dec, ValueProp props, Creature, CardModel,
                                   const CardPlay*) const {
        if (target != Owner() || !IsPoweredAttack(props)) return Dec(1);
        return DynamicVars()["DamageDecrease"].BaseValue() / Dec(100);
    }
};

}  // namespace sts2
