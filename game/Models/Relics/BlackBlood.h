// Models/Relics/BlackBlood.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BlackBlood : RelicModel {
    SIM_MODEL(BlackBlood, RelicModel)

    void AfterCombatVictory(CombatRoom) const {
        if (Owner().Creature().IsDead()) return;
        Flash();
        CreatureCmd::Heal(Owner().Creature(), DynamicVars().Heal().BaseValue());
    }
};

}  // namespace sts2
