// Models/Cards/Offering.cs
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/CardPileCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Offering : CardModel { SIM_MODEL(Offering, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { CreatureCmd::Damage(ctx,Owner().Creature(),DynamicVars().HpLoss().BaseValue(),ValueProp::Unblockable|ValueProp::Unpowered|ValueProp::Move,*this,p); PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(),Owner()); CardPileCmd::Draw(ctx,DynamicVars().Cards().BaseValue(),Owner()); }
};
}
