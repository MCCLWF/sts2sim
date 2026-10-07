// Models/Cards/Uppercut.cs
#include "game/Commands/PowerCmd.h"
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Uppercut : CardModel { SIM_MODEL(Uppercut, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).Targeting(p->Target).Execute(ctx); Dec n=DynamicVars()["Power"].BaseValue(); PowerCmd::Apply(PowerId::WEAK_POWER,ctx,p->Target,n,Owner().Creature(),*this); PowerCmd::Apply(PowerId::VULNERABLE_POWER,ctx,p->Target,n,Owner().Creature(),*this); }
};
}
