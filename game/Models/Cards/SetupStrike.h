// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct SetupStrike : CardModel { SIM_MODEL(SetupStrike, CardModel)
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const { if (!play || !play->Target) throw GameError("cardPlay.Target is null"); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx); PowerCmd::Apply(PowerId::SETUP_STRIKE_POWER, ctx, Owner().Creature(), DynamicVars().Strength().BaseValue(), Owner().Creature(), *this); }
};
}
