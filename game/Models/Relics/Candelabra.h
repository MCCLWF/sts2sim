// Models/Relics/Candelabra.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Candelabra : RelicModel {
    SIM_MODEL(Candelabra, RelicModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature()) && Owner().PlayerCombatState().TurnNumber() == 2)
            PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }
};

}  // namespace sts2
