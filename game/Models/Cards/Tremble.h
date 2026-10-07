// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Tremble : CardModel { SIM_MODEL(Tremble, CardModel)
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const { if (!play || !play->Target) throw GameError("cardPlay.Target is null"); PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, play->Target, DynamicVars().Vulnerable().BaseValue(), Owner().Creature(), *this); }
};
}
