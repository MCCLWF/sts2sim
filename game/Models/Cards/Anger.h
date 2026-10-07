// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Anger : CardModel { SIM_MODEL(Anger, CardModel)
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
  if (!play || !play->Target) throw GameError("cardPlay.Target is null");
  DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
  CardPileCmd::AddGeneratedCardToCombat(CreateClone(), PileType::Discard, Owner());
 }
};
}
