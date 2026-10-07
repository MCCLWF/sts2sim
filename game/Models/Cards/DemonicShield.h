// Models/Cards/DemonicShield.cs
#include "game/Commands/CreatureCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct DemonicShield : CardModel { SIM_MODEL(DemonicShield, CardModel)
bool GainsBlock() const { return true; }
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); CreatureCmd::Damage(ctx,Owner().Creature(),DynamicVars().HpLoss().BaseValue(),ValueProp::Unblockable|ValueProp::Unpowered|ValueProp::Move,*this,p); CreatureCmd::GainBlock(p->Target,DynamicVars().CalculatedBlock().Calculate(p->Target),DynamicVars().CalculatedBlock().Props(),p); }
};
}
