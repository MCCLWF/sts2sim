// Models/Cards/Dominate.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Dominate : CardModel { SIM_MODEL(Dominate, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); PowerCmd::Apply(PowerId::VULNERABLE_POWER,ctx,p->Target,DynamicVars()["VulnerablePower"].BaseValue(),Owner().Creature(),*this); PowerCmd::Apply(PowerId::STRENGTH_POWER,ctx,Owner().Creature(),Dec(p->Target.GetPowerAmount(PowerId::VULNERABLE_POWER)),Owner().Creature(),*this); }
};
}
