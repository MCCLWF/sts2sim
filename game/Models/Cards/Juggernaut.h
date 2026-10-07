// Models/Cards/Juggernaut.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Juggernaut : CardModel { SIM_MODEL(Juggernaut, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::JUGGERNAUT_POWER,ctx,Owner().Creature(),DynamicVars()["JuggernautPower"].BaseValue(),Owner().Creature(),*this); }
};
}
