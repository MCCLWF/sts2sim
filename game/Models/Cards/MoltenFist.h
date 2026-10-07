// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct MoltenFist : CardModel { SIM_MODEL(MoltenFist, CardModel)
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
  if (!play || !play->Target) throw GameError("cardPlay.Target is null");
  DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
  int32_t n = play->Target.IsAlive() ? play->Target.GetPowerAmount(PowerId::VULNERABLE_POWER) : 0;
  if (n > 0) PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, play->Target, Dec(n), Owner().Creature(), *this);
 }
};
}
