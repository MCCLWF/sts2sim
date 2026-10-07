// Models/Relics/Pear.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Pear : RelicModel {
    SIM_MODEL(Pear, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { CreatureCmd::GainMaxHp(Owner().Creature(), DynamicVars().MaxHp().BaseValue()); }
};

}  // namespace sts2
