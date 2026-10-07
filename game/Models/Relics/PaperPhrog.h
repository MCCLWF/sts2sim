// Models/Relics/PaperPhrog.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct PaperPhrog : RelicModel {
    SIM_MODEL(PaperPhrog, RelicModel)

    Dec ModifyVulnerableMultiplier(Creature target, Dec amount, ValueProp props,
                                   Creature, CardModel) const {
        if (target == Owner().Creature() || !IsPoweredAttack(props)) return amount;
        return amount + Dec::lit(25, 2);
    }
};

}  // namespace sts2
