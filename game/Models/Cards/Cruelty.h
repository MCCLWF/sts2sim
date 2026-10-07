// Models/Cards/Cruelty.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Cruelty : CardModel { SIM_MODEL(Cruelty, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::CRUELTY_POWER,ctx,Owner().Creature(),DynamicVars()["CrueltyPower"].BaseValue(),Owner().Creature(),*this); }
};
}
