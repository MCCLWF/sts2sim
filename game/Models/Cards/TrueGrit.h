// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct TrueGrit : CardModel { SIM_MODEL(TrueGrit, CardModel)
 bool GainsBlock() const { return true; }
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
  CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
  CardModel card;
  if (CurrentUpgradeLevel() > 0) { CardList cards = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(1), nullptr, *this); if (!cards.empty()) card = cards[0]; }
  else { CardList hand = CardPile::Get(PileType::Hand, Owner()).Cards(); card = Owner().RunState().Rng().CombatCardSelection().NextItem(hand); }
  if (card) CardCmd::Exhaust(ctx, card);
 }
};
}
