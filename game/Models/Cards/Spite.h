// Models/Cards/Spite.cs
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Spite : CardModel { SIM_MODEL(Spite, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); int hits=CombatState().st().player_lost_hp_this_turn?DynamicVars().Repeat().IntValue():1; DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(hits).FromCard(*this,p).Targeting(p->Target).Execute(ctx); }
};
}
