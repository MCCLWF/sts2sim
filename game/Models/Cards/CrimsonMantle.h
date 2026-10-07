// Models/Cards/CrimsonMantle.cs
#include "game/Commands/PowerCmd.h"
#include "game/Models/Powers/CrimsonMantlePower.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct CrimsonMantle : CardModel { SIM_MODEL(CrimsonMantle, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerModel p=PowerCmd::Apply(PowerId::CRIMSON_MANTLE_POWER,ctx,Owner().Creature(),DynamicVars()["CrimsonMantlePower"].BaseValue(),Owner().Creature(),*this); if(p) CrimsonMantlePower(p).IncrementSelfDamage(); }
};
}
