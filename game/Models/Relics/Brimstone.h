// Models/Relics/Brimstone.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Brimstone : RelicModel {
    SIM_MODEL(Brimstone, RelicModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState combatState) const {
        Creature owner = Owner().Creature();
        if (!participants.contains(owner)) return;

        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, owner,
                        DynamicVars()["SelfStrength"].BaseValue(), owner, CardModel{});
        CreatureList targets;
        for (Creature opponent : combatState.GetOpponentsOf(owner))
            if (opponent.IsAlive()) targets.push_back(opponent);
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, targets,
                        DynamicVars()["EnemyStrength"].BaseValue(), Creature{}, CardModel{});
    }
};

}  // namespace sts2
