// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Headbutt : CardModel { SIM_MODEL(Headbutt, CardModel)
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
  if (!play || !play->Target) throw GameError("cardPlay.Target is null");
  DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
  CardList cards = CardSelectCmd::FromCombatPile(ctx, CardPile::Get(PileType::Discard, Owner()), Owner(), CardSelectorPrefs(1));
  if (!cards.empty()) CardPileCmd::Add(cards[0], PileType::Draw, CardPilePosition::Top);
 }
};
}
