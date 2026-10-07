// Models/Cards/Whirlwind.cs
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Whirlwind : CardModel { SIM_MODEL(Whirlwind, CardModel)
bool HasEnergyCostX() const { return true; }
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { int32_t n=ResolveEnergyXValue(); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(n).FromCard(*this,p).TargetingAllOpponents(CombatState()).Execute(ctx); }
};
}
