// Models/Powers/DisintegrationPower.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct DisintegrationPower : PowerModel {
    SIM_MODEL(DisintegrationPower, PowerModel)
    void AfterSideTurnEndLate(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) CreatureCmd::Damage(ctx, Owner(), Dec(Amount()), ValueProp::Unpowered, Owner());
    }
};
}  // namespace sts2
