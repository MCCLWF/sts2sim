// Models/Cards/Break.cs
#include "game/Commands/PowerCmd.h"
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Break : CardModel { SIM_MODEL(Break, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).Targeting(p->Target).Execute(ctx); PowerCmd::Apply(PowerId::VULNERABLE_POWER,ctx,p->Target,DynamicVars().Vulnerable().BaseValue(),Owner().Creature(),*this); }
};
}
