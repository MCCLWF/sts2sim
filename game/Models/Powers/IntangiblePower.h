// Models/Powers/IntangiblePower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct IntangiblePower : PowerModel {
    SIM_MODEL(IntangiblePower, PowerModel)

    Dec ModifyHpLostAfterOsty(Creature target, Dec amount, ValueProp, Creature, CardModel) const {
        return target == Owner() && amount >= Dec(1) ? Dec(1) : amount;
    }

    Dec ModifyDamageCap(Creature target, ValueProp, Creature, CardModel, const CardPlay*) const {
        return target == Owner() ? Dec(1) : Dec::max_value();
    }

    void AfterModifyingHpLostAfterOsty() const {}
    void AfterModifyingDamageAmount(CardModel) const {}

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide side, const CreatureList&) const {
        if (side == CombatSide::Enemy) PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
