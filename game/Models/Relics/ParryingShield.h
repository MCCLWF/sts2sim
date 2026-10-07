// Models/Relics/ParryingShield.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct ParryingShield : RelicModel {
    SIM_MODEL(ParryingShield, RelicModel)
    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner().Creature()) ||
            Dec(Owner().Creature().Block()) < DynamicVars().Block().BaseValue()) return;
        CreatureList enemies = Owner().Creature().CombatState().HittableEnemies();
        Creature target = Owner().RunState().Rng().CombatTargets().NextItem(enemies);
        if (target) CreatureCmd::Damage(ctx, target, DynamicVars().Damage(), Owner().Creature());
    }
};

}  // namespace sts2
