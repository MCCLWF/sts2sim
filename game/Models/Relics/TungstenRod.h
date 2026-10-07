#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct TungstenRod : RelicModel {
    SIM_MODEL(TungstenRod, RelicModel)

    Dec ModifyHpLostAfterOsty(Creature target, Dec amount, ValueProp, Creature, CardModel) const {
        if (target != Owner().Creature()) return amount;
        Dec reduction = DynamicVars()["HpLossReduction"].BaseValue();
        return amount > reduction ? amount - reduction : Dec(0);
    }

    void AfterModifyingHpLostAfterOsty() const { Flash(); }
};

}  // namespace sts2
