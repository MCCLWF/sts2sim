#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BagOfMarbles : RelicModel {
    SIM_MODEL(BagOfMarbles, RelicModel)

    void BeforeSideTurnStart(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants,
                              CombatState combatState) const {
        if (!participants.contains(Owner().Creature()) || !Owner().PlayerCombatState() ||
            Owner().PlayerCombatState().TurnNumber() > 1) return;
        Flash();
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, combatState.HittableEnemies(),
                        DynamicVars().Vulnerable().BaseValue(), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
