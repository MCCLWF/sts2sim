#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Chandelier : RelicModel {
    SIM_MODEL(Chandelier, RelicModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature()) && Owner().PlayerCombatState() &&
            Owner().PlayerCombatState().TurnNumber() == 3)
            PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }
};

}  // namespace sts2
