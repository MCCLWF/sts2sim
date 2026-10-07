// Models/Cards/DrumOfBattle.cs
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PlayerCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct DrumOfBattle : CardModel { SIM_MODEL(DrumOfBattle, CardModel)
void AfterCardExhausted(PlayerChoiceContext&, CardModel card, bool) const {
  if (card != *this || !CombatState()) return;
  int32_t playCount = GeneratePlayCount(CombatState(), Creature{});
  for (int32_t i = 0; i < playCount; ++i)
    PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
}
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { CardPileCmd::Draw(ctx,DynamicVars().Cards().BaseValue(),Owner()); }
};
}
