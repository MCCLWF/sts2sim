// Models/Cards/Unrelenting.cs
#include "game/Commands/PowerCmd.h"
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Unrelenting : CardModel { SIM_MODEL(Unrelenting, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).Targeting(p->Target).Execute(ctx); PowerCmd::Apply(PowerId::FREE_ATTACK_POWER,ctx,Owner().Creature(),Dec(1),Owner().Creature(),*this); }
};
}
