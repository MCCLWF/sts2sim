// Models/Cards/Bloodletting.cs
#include "game/Commands/CreatureCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Bloodletting : CardModel { SIM_MODEL(Bloodletting, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { CreatureCmd::Damage(ctx,Owner().Creature(),DynamicVars().HpLoss().BaseValue(),ValueProp::Unblockable|ValueProp::Unpowered|ValueProp::Move,*this,p); PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(),Owner()); }
};
}
