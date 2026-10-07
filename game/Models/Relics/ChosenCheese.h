#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {
struct ChosenCheese : RelicModel {
    SIM_MODEL(ChosenCheese, RelicModel)
    void AfterCombatEnd(CombatRoom) const { CreatureCmd::GainMaxHp(Owner().Creature(), DynamicVars().MaxHp().BaseValue()); }
};
}  // namespace sts2
