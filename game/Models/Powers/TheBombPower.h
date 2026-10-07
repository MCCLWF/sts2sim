#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct TheBombPower : PowerModel {
    SIM_MODEL(TheBombPower, PowerModel)

    void SetDamage(Dec damage) const { DynamicVars().Damage().SetBaseValue(damage); }

    void BeforeSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        if (Amount() > 1) {
            PowerCmd::Decrement(*this);
            return;
        }
        CreatureList enemies = CombatState().HittableEnemies();
        if (!enemies.empty()) CreatureCmd::Damage(ctx, enemies, DynamicVars().Damage(), Owner());
        PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
