// Models/Powers/ConstrictPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct ConstrictPower : PowerModel {
    SIM_MODEL(ConstrictPower, PowerModel)

    void AfterSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide side, const CreatureList& participants) const {
        if (participants.contains(Owner())) {
            CreatureCmd::Damage(choiceContext, Owner(), Dec(Amount()), ValueProp::Unpowered, Owner());
        }
    }

    void AfterDeath(PlayerChoiceContext& choiceContext, Creature creature, bool wasRemovalPrevented, float deathAnimLength) const {
        if (!wasRemovalPrevented && creature == Applier()) PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
