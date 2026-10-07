#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct BufferPower : PowerModel {
    SIM_MODEL(BufferPower, PowerModel)

    Dec ModifyHpLostAfterOstyLate(Creature target, Dec amount, ValueProp, Creature, CardModel) const {
        return target == Owner() ? Dec(0) : amount;
    }

    void AfterModifyingHpLostAfterOsty() const { PowerCmd::Decrement(*this); }

    Dec GetScaledAmountForMultiplayer(sts2::CombatState combatState, Creature, Dec amount, Creature, CardModel) const {
        return Dec(static_cast<int32_t>((combatState.Players().size() - 1) * 2 + 1)) * amount;
    }
};

}  // namespace sts2
