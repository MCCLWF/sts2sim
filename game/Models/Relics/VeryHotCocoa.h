#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct VeryHotCocoa : RelicModel {
    SIM_MODEL(VeryHotCocoa, RelicModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature()) || !Owner().PlayerCombatState() ||
            Owner().PlayerCombatState().TurnNumber() > 1) return;
        Flash();
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }
};

}  // namespace sts2
