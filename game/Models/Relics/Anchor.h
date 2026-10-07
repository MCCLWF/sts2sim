// Models/Relics/Anchor.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct Anchor : RelicModel {
    SIM_MODEL(Anchor, RelicModel)
    void BeforeCombatStart() const { CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr); }
};
}
