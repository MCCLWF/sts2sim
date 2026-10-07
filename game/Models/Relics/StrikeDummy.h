// Models/Relics/StrikeDummy.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct StrikeDummy : RelicModel {
    SIM_MODEL(StrikeDummy, RelicModel)

    Dec ModifyDamageAdditive(Creature target, Dec, ValueProp props, Creature dealer, CardModel cardSource,
                             const CardPlay*) const {
        if (!IsPoweredAttack(props) || !cardSource) return Dec(0);
        if (!cardSource.HasTag(CardTag::Strike)) return Dec(0);
        if (dealer != Owner().Creature() && cardSource.Owner() != Owner()) return Dec(0);
        return DynamicVars()["ExtraDamage"].BaseValue();
    }
};

}  // namespace sts2
