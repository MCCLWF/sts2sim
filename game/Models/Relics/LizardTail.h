// Models/Relics/LizardTail.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct LizardTail : RelicModel {
    SIM_MODEL(LizardTail, RelicModel)

    bool IsUsedUp() const { return extra(0) != 0; }

    bool ShouldDieLate(Creature creature) const {
        return creature != Owner().Creature() || IsUsedUp();
    }

    void AfterPreventingDeath(Creature creature) const {
        extra(0) = 1;
        SetStatus(RelicStatus::Disabled);
        Dec healFraction = DynamicVars().Heal().BaseValue() / Dec(100);
        Dec healAmount = Dec(creature.MaxHp()) * healFraction;
        if (healAmount < Dec(1)) healAmount = Dec(1);
        CreatureCmd::Heal(creature, healAmount);
    }
};

}  // namespace sts2
