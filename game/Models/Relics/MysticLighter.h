// Models/Relics/MysticLighter.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct MysticLighter : RelicModel {
    SIM_MODEL(MysticLighter, RelicModel)
    Dec ModifyDamageAdditive(Creature, Dec, ValueProp props, Creature, CardModel cardSource, const CardPlay*) const {
        if (!IsPoweredAttack(props) || !cardSource || !cardSource.Enchantment() || cardSource.Owner() != Owner())
            return Dec(0);
        return Dec(DynamicVars().Damage().IntValue());
    }
};

}  // namespace sts2
