// Models/Cards/HowlFromBeyond.cs
#include "game/Commands/DamageCmd.h"
#include "game/Commands/CardCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct HowlFromBeyond : CardModel { SIM_MODEL(HowlFromBeyond, CardModel)
void AfterAutoPostPlayPhaseEntered(PlayerChoiceContext& ctx, Player player) const {
  CardPile pile = Pile();
  if (pile && pile.Type() == PileType::Exhaust && player == Owner()) CardCmd::AutoPlay(ctx, *this, Creature{});
}
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).TargetingAllOpponents(CombatState()).Execute(ctx); }
};
}
