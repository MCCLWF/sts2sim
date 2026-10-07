// Models/Cards/Stampede.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Stampede : CardModel { SIM_MODEL(Stampede, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::STAMPEDE_POWER,ctx,Owner().Creature(),DynamicVars()["Power"].BaseValue(),Owner().Creature(),*this); }
};
}
