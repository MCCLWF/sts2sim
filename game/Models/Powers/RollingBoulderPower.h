#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct RollingBoulderPower : PowerModel {
    SIM_MODEL(RollingBoulderPower, PowerModel)

    void AfterPlayerTurnStart(PlayerChoiceContext& ctx, Player player) const {
        if (player != Owner().Player()) return;
        CreatureCmd::Damage(ctx, CombatState().HittableEnemies(), Dec(Amount()), ValueProp::Unpowered, Owner());
        SetAmount(Amount() + DynamicVars().Damage().IntValue());
    }
};

}  // namespace sts2
