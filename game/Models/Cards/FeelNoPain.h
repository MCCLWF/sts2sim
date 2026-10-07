// Models/Cards/FeelNoPain.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct FeelNoPain : CardModel { SIM_MODEL(FeelNoPain, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::FEEL_NO_PAIN_POWER,ctx,Owner().Creature(),DynamicVars()["Power"].BaseValue(),Owner().Creature(),*this); }
};
}
