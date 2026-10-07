#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct TwistedFunnel : RelicModel {
    SIM_MODEL(TwistedFunnel, RelicModel)

    void BeforeSideTurnStart(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants, CombatState combatState) const {
        if (!participants.contains(Owner().Creature()) || Owner().PlayerCombatState().TurnNumber() > 1) return;
        for (Creature enemy : combatState.HittableEnemies())
            PowerCmd::Apply(PowerId::POISON_POWER, ctx, enemy, DynamicVars().Poison().BaseValue(), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
