// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct IronWave : CardModel { SIM_MODEL(IronWave, CardModel)
 bool GainsBlock() const { return true; }
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
  if (!play || !play->Target) throw GameError("cardPlay.Target is null");
  CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
  DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
 }
};
}
