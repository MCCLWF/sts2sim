// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Thunderclap : CardModel { SIM_MODEL(Thunderclap, CardModel)
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const { DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).TargetingAllOpponents(CombatState()).Execute(ctx); PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, CombatState().HittableEnemies(), DynamicVars().Vulnerable().BaseValue(), Owner().Creature(), *this); }
};
}
