// Models/Relics/FakeAnchor.cs (v0.111.0).
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct FakeAnchor : RelicModel {
    SIM_MODEL(FakeAnchor, RelicModel)
    int32_t MerchantCost() const { return 50; }
    void BeforeCombatStart() const { CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr); }
};
}
