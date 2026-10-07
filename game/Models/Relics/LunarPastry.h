#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {
struct LunarPastry : RelicModel {
    SIM_MODEL(LunarPastry, RelicModel)
    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner().Creature())) PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
    }
};
}  // namespace sts2
