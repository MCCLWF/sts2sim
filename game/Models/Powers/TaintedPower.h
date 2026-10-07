// Models/Powers/TaintedPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct TaintedPower : PowerModel {
    SIM_MODEL(TaintedPower, PowerModel)

    Dec ModifyDamageAdditive(Creature target, Dec, ValueProp props, Creature, CardModel, const CardPlay*) const {
        return target == Owner() && IsPoweredAttack(props) ? Dec(Amount()) : Dec(0);
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide side, const CreatureList&) const {
        if (side == CombatSide::Enemy) PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
