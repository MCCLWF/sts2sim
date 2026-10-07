#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct DebilitatePower : PowerModel {
    SIM_MODEL(DebilitatePower, PowerModel)
    Dec ModifyVulnerableMultiplier(Creature target, Dec amount, ValueProp props, Creature, CardModel) const {
        return target == Owner() && IsPoweredAttack(props) ? amount + (amount - Dec(1)) : amount;
    }
    Dec ModifyWeakMultiplier(Creature, Dec amount, ValueProp props, Creature dealer, CardModel) const {
        return dealer == Owner() && IsPoweredAttack(props) ? amount - (Dec(1) - amount) : amount;
    }
    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
