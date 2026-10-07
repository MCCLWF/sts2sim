// Models/Cards/Thrash.cs
#include "game/Commands/CardCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Hooks/Hook.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Thrash : CardModel { SIM_MODEL(Thrash, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const {
  if(!p||!p->Target) throw GameError("cardPlay.Target is null");
  DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(2).FromCard(*this,p).Targeting(p->Target).Execute(ctx);
  CardList attacks; for(CardModel c:CardPile::Get(PileType::Hand,Owner()).Cards()) if(c.Type()==CardType::Attack) attacks.push_back(c);
  CardModel chosen = Owner().RunState().Rng().CombatCardSelection().NextItem(attacks);
  if(!chosen) return;
  Dec damage;
  if(chosen.DynamicVars().ContainsKey("CalculatedDamage")) damage=chosen.DynamicVars().CalculatedDamage().Calculate(Creature{});
  else if(chosen.DynamicVars().ContainsKey("Damage")) damage=chosen.DynamicVars().Damage().BaseValue();
  else if(chosen.DynamicVars().ContainsKey("OstyDamage")) damage=chosen.DynamicVars().OstyDamage().BaseValue();
  ModelList modifiers;
  damage=Hook::ModifyDamage(Owner().RunState(),CombatState(),Creature{},Owner().Creature(),damage,ValueProp::Move,chosen,nullptr,ModifyDamageHookType::All,CardPreviewMode::None,modifiers);
  DynamicVars().Damage().SetBaseValue(DynamicVars().Damage().BaseValue()+damage);
  SetGrowthDamage(GrowthDamage()+damage);
  CardCmd::Exhaust(ctx,chosen);
}
void AfterDowngraded() const { DynamicVars().Damage().SetBaseValue(DynamicVars().Damage().BaseValue()+GrowthDamage()); }
};
}
