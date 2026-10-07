// Models/Relics/Lantern.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Lantern : RelicModel {
    SIM_MODEL(Lantern, RelicModel)
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature()) && Owner().PlayerCombatState().TurnNumber() <= 1)
            PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }
};

}  // namespace sts2
