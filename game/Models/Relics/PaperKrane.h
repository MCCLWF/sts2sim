// Models/Relics/PaperKrane.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct PaperKrane : RelicModel {
    SIM_MODEL(PaperKrane, RelicModel)

    Dec ModifyWeakMultiplier(Creature target, Dec amount, ValueProp props, Creature, CardModel) const {
        if (target != Owner().Creature()) return amount;
        if (!IsPoweredAttack(props)) return amount;
        return amount - Dec::lit(15, 2);
    }
};

}  // namespace sts2
