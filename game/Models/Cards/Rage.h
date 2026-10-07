// Models/Cards/Rage.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Rage : CardModel { SIM_MODEL(Rage, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::RAGE_POWER,ctx,Owner().Creature(),DynamicVars()["Power"].BaseValue(),Owner().Creature(),*this); }
};
}
