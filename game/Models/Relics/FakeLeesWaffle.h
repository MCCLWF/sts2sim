// Models/Relics/FakeLeesWaffle.cs (v0.111.0).
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct FakeLeesWaffle : RelicModel {
    SIM_MODEL(FakeLeesWaffle, RelicModel)
    int32_t MerchantCost() const { return 50; }
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const {
        Creature owner = Owner().Creature();
        CreatureCmd::Heal(owner, Dec(owner.MaxHp()) * (DynamicVars().Heal().BaseValue() / Dec(100)));
    }
};
}
