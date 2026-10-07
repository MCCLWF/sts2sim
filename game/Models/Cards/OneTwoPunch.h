// Models/Cards/OneTwoPunch.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct OneTwoPunch : CardModel { SIM_MODEL(OneTwoPunch, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::ONE_TWO_PUNCH_POWER,ctx,Owner().Creature(),DynamicVars()["Attacks"].BaseValue(),Owner().Creature(),*this); }
};
}
