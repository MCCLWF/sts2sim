// Models/Cards/PrimalForce.cs
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/CardCmd.h"
namespace sts2 {
struct PrimalForce : CardModel { SIM_MODEL(PrimalForce, CardModel)
void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
  CardList hand = CardPile::Get(PileType::Hand, Owner()).Cards();
  for (CardModel card : hand) {
    if (card.Type() != CardType::Attack || !card.IsTransformable()) continue;
    CardModel replacement = Owner().Creature().CombatState().CreateCard(CardId::GIANT_ROCK, Owner());
    if (CurrentUpgradeLevel() > 0) CardCmd::Upgrade(replacement);
    CardCmd::Transform(card, replacement);
  }
}
};
}
