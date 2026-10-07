// Models/Cards/Rampage.cs
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Rampage : CardModel { SIM_MODEL(Rampage, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const {
    if (!p || !p->Target) throw GameError("cardPlay.Target is null");
    DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).Targeting(p->Target).Execute(ctx);
    Dec increase = DynamicVars()["Increase"].BaseValue();
    DynamicVars()["Damage"].SetBaseValue(DynamicVars().Damage().BaseValue() + increase);
    SetGrowthDamage(GrowthDamage() + increase);
}
void AfterDowngraded() const {
    DynamicVars()["Damage"].SetBaseValue(DynamicVars().Damage().BaseValue() + GrowthDamage());
}
};
}
