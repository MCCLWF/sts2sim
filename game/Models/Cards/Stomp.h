// Models/Cards/Stomp.cs
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Stomp : CardModel { SIM_MODEL(Stomp, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).TargetingAllOpponents(CombatState()).Execute(ctx); }
void AfterCardEnteredCombat(CardModel card) const {
  if (card != *this || st().is_dupe || st().clone_of != 0) return;
  EnergyCost().AddThisTurn(-CombatState().st().player_attack_cards_this_turn);
}
void BeforeCardPlayed(const CardPlay* play) const {
  if (play && play->Player == Owner() && play->Card.Type() == CardType::Attack) EnergyCost().AddThisTurn(-1);
}
};
}
