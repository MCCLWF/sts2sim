// Models/Cards/Brand.cs
#include "game/Commands/PowerCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/CardCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Brand : CardModel { SIM_MODEL(Brand, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { CreatureCmd::Damage(ctx,Owner().Creature(),DynamicVars().HpLoss().BaseValue(),ValueProp::Unblockable|ValueProp::Unpowered|ValueProp::Move,*this,p); CardList cards=CardSelectCmd::FromHand(ctx,Owner(),CardSelectorPrefs(1),nullptr,*this); if(!cards.empty()) CardCmd::Exhaust(ctx,cards[0]); PowerCmd::Apply(PowerId::STRENGTH_POWER,ctx,Owner().Creature(),DynamicVars().Strength().BaseValue(),Owner().Creature(),*this); }
};
}
