// Models/Relics/FakeMango.cs (v0.111.0).
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct FakeMango : RelicModel {
    SIM_MODEL(FakeMango, RelicModel)
    int32_t MerchantCost() const { return 50; }
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { CreatureCmd::GainMaxHp(Owner().Creature(), DynamicVars().MaxHp().BaseValue()); }
};
}
