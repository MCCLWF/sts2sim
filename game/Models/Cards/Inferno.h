// Models/Cards/Inferno.cs
#include "game/Commands/PowerCmd.h"
#include "game/Models/Powers/InfernoPower.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Inferno : CardModel { SIM_MODEL(Inferno, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerModel p=PowerCmd::Apply(PowerId::INFERNO_POWER,ctx,Owner().Creature(),DynamicVars()["InfernoPower"].BaseValue(),Owner().Creature(),*this); if(p) InfernoPower(p).IncrementSelfDamage(); }
};
}
