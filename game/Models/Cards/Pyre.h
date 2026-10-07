// Models/Cards/Pyre.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Pyre : CardModel { SIM_MODEL(Pyre, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::PYRE_POWER,ctx,Owner().Creature(),DynamicVars().Energy().BaseValue(),Owner().Creature(),*this); }
};
}
