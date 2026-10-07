// Models/Cards/Mangle.cs
#include "game/Commands/PowerCmd.h"
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Mangle : CardModel { SIM_MODEL(Mangle, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).Targeting(p->Target).Execute(ctx); PowerCmd::Apply(PowerId::MANGLE_POWER,ctx,p->Target,DynamicVars()["StrengthLoss"].BaseValue(),Owner().Creature(),*this); }
};
}
