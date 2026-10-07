// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Taunt : CardModel { SIM_MODEL(Taunt, CardModel)
 bool GainsBlock() const { return true; }
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const { if (!play || !play->Target) throw GameError("cardPlay.Target is null"); CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play); PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, play->Target, DynamicVars().Vulnerable().BaseValue(), Owner().Creature(), *this); }
};
}
