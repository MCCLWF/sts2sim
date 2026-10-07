// Models/Cards/Vicious.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Vicious : CardModel { SIM_MODEL(Vicious, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::VICIOUS_POWER,ctx,Owner().Creature(),DynamicVars().Cards().BaseValue(),Owner().Creature(),*this); }
};
}
