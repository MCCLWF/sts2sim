// Models/Cards/Blaze.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Blaze : CardModel { SIM_MODEL(Blaze, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if (!p||!p->Target) throw GameError("cardPlay.Target is null"); PowerCmd::Apply(PowerId::STRENGTH_POWER,ctx,p->Target,DynamicVars().Strength().BaseValue(),Owner().Creature(),*this); }
};
}
