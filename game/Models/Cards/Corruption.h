// Models/Cards/Corruption.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Corruption : CardModel { SIM_MODEL(Corruption, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::CORRUPTION_POWER,ctx,Owner().Creature(),DynamicVars()["Power"].BaseValue(),Owner().Creature(),*this); }
};
}
