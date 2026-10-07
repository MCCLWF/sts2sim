// Models/Cards/Inflame.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Inflame : CardModel { SIM_MODEL(Inflame, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::STRENGTH_POWER,ctx,Owner().Creature(),DynamicVars()["StrengthPower"].BaseValue(),Owner().Creature(),*this); }
};
}
