// Models/Cards/FightMe.cs
#include "game/Commands/PowerCmd.h"
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct FightMe : CardModel { SIM_MODEL(FightMe, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(DynamicVars().Repeat().IntValue()).FromCard(*this,p).Targeting(p->Target).Execute(ctx); PowerCmd::Apply(PowerId::STRENGTH_POWER,ctx,Owner().Creature(),DynamicVars()["StrengthPower"].BaseValue(),Owner().Creature(),*this); PowerCmd::Apply(PowerId::STRENGTH_POWER,ctx,p->Target,DynamicVars()["EnemyStrength"].BaseValue(),Owner().Creature(),*this); }
};
}
