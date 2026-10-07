#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct TheGambitPower : PowerModel {
    SIM_MODEL(TheGambitPower, PowerModel)

    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult& result, ValueProp props,
                             Creature, CardModel) const {
        if (target != Owner() || !IsPoweredAttack(props) || result.UnblockedDamage <= 0) return;
        PowerCmd::Remove(*this);
        CreatureCmd::Kill(Owner());
    }
};

}  // namespace sts2
