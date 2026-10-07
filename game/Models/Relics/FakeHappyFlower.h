// Models/Relics/FakeHappyFlower.cs (v0.111.0).
#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct FakeHappyFlower : RelicModel {
    SIM_MODEL(FakeHappyFlower, RelicModel)
    int32_t MerchantCost() const { return 50; }
    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return extra(0); }
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature())) return;
        extra(0) = (extra(0) + 1) % DynamicVars()["Turns"].IntValue();
        SetStatus(extra(0) == DynamicVars()["Turns"].IntValue() - 1 ? RelicStatus::Active : RelicStatus::Normal);
        if (extra(0) == 0) PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }
    void AfterCombatEnd(CombatRoom) const { SetStatus(RelicStatus::Normal); }
};
}
