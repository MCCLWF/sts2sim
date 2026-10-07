#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Bread : RelicModel {
    SIM_MODEL(Bread, RelicModel)

    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        if (player != Owner() || (Owner().PlayerCombatState() && Owner().PlayerCombatState().TurnNumber() == 1)) return amount;
        return amount + DynamicVars()["GainEnergy"].BaseValue();
    }

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature()) && Owner().PlayerCombatState() &&
            Owner().PlayerCombatState().TurnNumber() == 1)
            PlayerCmd::LoseEnergy(DynamicVars()["LoseEnergy"].BaseValue(), Owner());
    }
};

}  // namespace sts2
