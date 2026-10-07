// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct BodySlam : CardModel { SIM_MODEL(BodySlam, CardModel)
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
  if (!play || !play->Target) throw GameError("cardPlay.Target is null");
  DamageCmd::Attack(DynamicVars().CalculatedDamage()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
 }
};
}
