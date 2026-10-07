// Models/Cards/Colossus.cs
#include "game/Commands/PowerCmd.h"
#include "game/Commands/CreatureCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Colossus : CardModel { SIM_MODEL(Colossus, CardModel)
bool GainsBlock() const { return true; }
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { CreatureCmd::GainBlock(Owner().Creature(),DynamicVars().Block(),p); PowerCmd::Apply(PowerId::COLOSSUS_POWER,ctx,Owner().Creature(),DynamicVars()["Colossus"].BaseValue(),Owner().Creature(),*this); }
};
}
