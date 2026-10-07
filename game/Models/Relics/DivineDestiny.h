// Models/Relics/DivineDestiny.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct DivineDestiny : RelicModel {
    SIM_MODEL(DivineDestiny, RelicModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature()) && Owner().PlayerCombatState().TurnNumber() <= 1)
            PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
    }
};

}  // namespace sts2
