// Models/Relics/RedMask.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct RedMask : RelicModel {
    SIM_MODEL(RedMask, RelicModel)
    void BeforeSideTurnStart(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants,
                             CombatState combatState) const {
        if (participants.contains(Owner().Creature()) && Owner().PlayerCombatState().TurnNumber() <= 1)
            PowerCmd::Apply(PowerId::WEAK_POWER, ctx, combatState.HittableEnemies(),
                            DynamicVars()["WeakPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
