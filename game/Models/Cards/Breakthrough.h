// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Breakthrough : CardModel { SIM_MODEL(Breakthrough, CardModel)
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
  CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars().HpLoss().BaseValue(), ValueProp::Unblockable|ValueProp::Unpowered|ValueProp::Move, *this, play);
  DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).TargetingAllOpponents(CombatState()).Execute(ctx);
 }
};
}
