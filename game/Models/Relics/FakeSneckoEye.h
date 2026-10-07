// Models/Relics/FakeSneckoEye.cs (v0.111.0).
#pragma once
#include "game/Combat/CombatManager.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct FakeSneckoEye : RelicModel {
    SIM_MODEL(FakeSneckoEye, RelicModel)
    int32_t MerchantCost() const { return 50; }
    void AfterObtained() const {
        if (CombatManager(sim_).IsInProgress()) ApplyPower();
    }
    void BeforeCombatStart() const { ApplyPower(); }
    void ApplyPower() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::CONFUSED_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), CardModel{});
    }
};
}
