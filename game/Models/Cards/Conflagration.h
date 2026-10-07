// Models/Cards/Conflagration.cs
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Conflagration : CardModel { SIM_MODEL(Conflagration, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(DynamicVars().Repeat().IntValue()).FromCard(*this,p).TargetingAllOpponents(CombatState()).Execute(ctx); }
};
}
