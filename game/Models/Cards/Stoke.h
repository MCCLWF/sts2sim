// Models/Cards/Stoke.cs
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Factories/CardFactory.h"
namespace sts2 {
struct Stoke : CardModel { SIM_MODEL(Stoke, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
  CardList hand = CardPile::Get(PileType::Hand, Owner()).Cards();
  int32_t n = static_cast<int32_t>(hand.size());
  for (CardModel card : hand) CardCmd::Exhaust(ctx, card);
  CardList cards = CardFactory::GetForCombat(Owner(), sts2::data(Owner().Character()).card_pool, n,
                                              Owner().RunState().Rng().CombatCardGeneration());
  if (CurrentUpgradeLevel() > 0) CardCmd::Upgrade(cards);
  CardPileCmd::AddGeneratedCardsToCombat(cards, PileType::Hand, Owner());
}
};
}
