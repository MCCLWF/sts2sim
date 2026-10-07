#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct DemisePower : PowerModel {
    SIM_MODEL(DemisePower, PowerModel)

    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        ExecutionSourceScope damageSource(Owner().sim(), ExecutionDamageSourceKind::Power, "DemisePower");
        CreatureCmd::Damage(ctx, Owner(), Dec(Amount()), ValueProp::Unblockable | ValueProp::Unpowered,
                             Creature{}, CardModel{}, nullptr);
    }
};

}  // namespace sts2
