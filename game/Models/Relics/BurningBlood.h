// Models/Relics/BurningBlood.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BurningBlood : RelicModel {
    SIM_MODEL(BurningBlood, RelicModel)

    void AfterCombatVictory(CombatRoom) const {
        if (!Owner().Creature().IsDead()) {
            Flash();
            CreatureCmd::Heal(Owner().Creature(), DynamicVars().Heal().BaseValue());
        }
    }
};

}  // namespace sts2
