// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/CardCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Cinder : CardModel { SIM_MODEL(Cinder, CardModel)
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
  if (!play || !play->Target) throw GameError("cardPlay.Target is null");
  DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
  CardList hand = CardPile::Get(PileType::Hand, Owner()).Cards();
  CardModel card = Owner().RunState().Rng().CombatCardSelection().NextItem(hand);
  if (card) CardCmd::Exhaust(ctx, card);
 }
};
}
