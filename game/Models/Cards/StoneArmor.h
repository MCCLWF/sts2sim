// Models/Cards/StoneArmor.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct StoneArmor : CardModel { SIM_MODEL(StoneArmor, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::PLATING_POWER,ctx,Owner().Creature(),DynamicVars()["PlatingPower"].BaseValue(),Owner().Creature(),*this); }
};
}
