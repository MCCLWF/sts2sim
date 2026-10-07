// Models/Relics/BloodVial.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BloodVial : RelicModel {
    SIM_MODEL(BloodVial, RelicModel)
    void AfterPlayerTurnStartLate(PlayerChoiceContext&, Player player) const {
        if (player == Owner() && Owner().PlayerCombatState().TurnNumber() <= 1)
            CreatureCmd::Heal(Owner().Creature(), DynamicVars().Heal().BaseValue());
    }
};

}  // namespace sts2
