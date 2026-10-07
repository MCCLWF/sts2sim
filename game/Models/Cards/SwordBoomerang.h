// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct SwordBoomerang : CardModel { SIM_MODEL(SwordBoomerang, CardModel)
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const { DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(DynamicVars().Repeat().IntValue()).FromCard(*this, play).TargetingRandomOpponents(CombatState()).Execute(ctx); }
};
}
