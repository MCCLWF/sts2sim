// Models/Cards/Midnight.cs
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Midnight : CardModel { SIM_MODEL(Midnight, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).Targeting(p->Target).Execute(ctx); }
void AfterCardEnteredCombat(CardModel card) const {
  if (card != *this || st().is_dupe || st().clone_of != 0) return;
  EnergyCost().AddThisCombat(-CombatState().st().cards_exhausted_this_combat);
}
void AfterCardExhausted(PlayerChoiceContext&, CardModel, bool) const { EnergyCost().AddThisCombat(-1); }
};
}
