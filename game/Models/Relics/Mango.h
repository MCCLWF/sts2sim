#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Mango : RelicModel {
    SIM_MODEL(Mango, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { CreatureCmd::GainMaxHp(Owner().Creature(), DynamicVars().MaxHp().BaseValue()); }
};

}  // namespace sts2
