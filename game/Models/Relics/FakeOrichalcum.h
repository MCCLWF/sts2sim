// Models/Relics/FakeOrichalcum.cs (v0.111.0).
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct FakeOrichalcum : RelicModel {
    SIM_MODEL(FakeOrichalcum, RelicModel)
    int32_t MerchantCost() const { return 50; }
    void BeforeSideTurnEndVeryEarly(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner().Creature()) && Owner().Creature().Block() <= Dec(0)) extra(0) = 1;
    }
    void BeforeSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList&) const {
        if (!extra(0)) return;
        extra(0) = 0;
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr);
    }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature())) extra(0) = 0;
    }
};
}
