// Models/Cards/ExpectAFight.cs
#include "game/Commands/CreatureCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct ExpectAFight : CardModel { SIM_MODEL(ExpectAFight, CardModel)
bool GainsBlock() const { return true; }
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { Dec block=DynamicVars().CalculatedBlock().Calculate(p ? p->Target : Creature{}); CreatureCmd::GainBlock(Owner().Creature(),block,DynamicVars().CalculatedBlock().Props(),p); }
};
}
