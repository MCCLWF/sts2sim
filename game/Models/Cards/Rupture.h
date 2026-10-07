// Models/Cards/Rupture.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Rupture : CardModel { SIM_MODEL(Rupture, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::RUPTURE_POWER,ctx,Owner().Creature(),DynamicVars().Strength().BaseValue(),Owner().Creature(),*this); }
};
}
