// Models/Cards/InfernalBlade.cs
#pragma once
#include "game/All.h"
#include "game/Factories/CardFactory.h"
#include "game/Commands/CardPileCmd.h"
namespace sts2 {
struct InfernalBlade : CardModel { SIM_MODEL(InfernalBlade, CardModel)
void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
  CardList cards = CardFactory::GetForCombat(Owner(), sts2::data(Owner().Character()).card_pool, 1,
                                              Owner().RunState().Rng().CombatCardGeneration(), CardType::Attack, true);
  if (!cards.empty()) {
   // SetToFreeThisTurn clears the temporary cost after this card is played as
   // well as at turn end; Infernal Blade's generated attack must not remain
   // free when it returns from discard during the same turn.
   cards[0].SetToFreeThisTurn();
   CardPileCmd::AddGeneratedCardToCombat(cards[0], PileType::Hand, Owner());
  }
}
};
}
