#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {
struct VitruvianMinion : RelicModel {
    SIM_MODEL(VitruvianMinion, RelicModel)
    Dec ModifyDamageMultiplicative(Creature, Dec, ValueProp, Creature, CardModel source, const CardPlay*) const {
        return source && source.Owner() == Owner() && source.HasTag(CardTag::Minion) ? Dec(2) : Dec(1);
    }
    Dec ModifyBlockMultiplicative(Creature, Dec, ValueProp, CardModel source, const CardPlay*) const {
        return source && source.Owner() == Owner() && source.HasTag(CardTag::Minion) ? Dec(2) : Dec(1);
    }
};
}  // namespace sts2
