#pragma once

#include "game/Models/RelicModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct MiniatureCannon : RelicModel {
    SIM_MODEL(MiniatureCannon, RelicModel)

    Dec ModifyDamageAdditive(Creature, Dec, ValueProp props, Creature dealer, CardModel cardSource,
                             const CardPlay*) const {
        if (!IsPoweredAttack(props) || !cardSource || cardSource.CurrentUpgradeLevel() == 0) return Dec(0);
        if (dealer != Owner().Creature() && cardSource.Owner() != Owner()) return Dec(0);
        return DynamicVars()["ExtraDamage"].BaseValue();
    }
};

}  // namespace sts2
