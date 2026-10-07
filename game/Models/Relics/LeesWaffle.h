#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct LeesWaffle : RelicModel {
    SIM_MODEL(LeesWaffle, RelicModel)
    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        Creature creature = Owner().Creature();
        CreatureCmd::GainMaxHp(creature, DynamicVars().MaxHp().BaseValue());
        CreatureCmd::Heal(creature, Dec(creature.MaxHp() - creature.CurrentHp()));
    }
};

}  // namespace sts2
