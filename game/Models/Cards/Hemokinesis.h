// Models/Cards/Hemokinesis.cs
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Hemokinesis : CardModel { SIM_MODEL(Hemokinesis, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); CreatureCmd::Damage(ctx,Owner().Creature(),DynamicVars().HpLoss().BaseValue(),ValueProp::Unblockable|ValueProp::Unpowered|ValueProp::Move,*this,p); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).Targeting(p->Target).Execute(ctx); }
};
}
