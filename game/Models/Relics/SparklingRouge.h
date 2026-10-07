// Models/Relics/SparklingRouge.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct SparklingRouge : RelicModel {
    SIM_MODEL(SparklingRouge, RelicModel)
    void AfterBlockCleared(Creature creature) const {
        if (creature != Owner().Creature() || Owner().PlayerCombatState().TurnNumber() != 3) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, creature, DynamicVars().Strength().BaseValue(), creature, CardModel{});
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, creature, DynamicVars().Dexterity().BaseValue(), creature, CardModel{});
    }
};

}  // namespace sts2
