// Models/Cards/Impervious.cs
#include "game/Commands/CreatureCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Impervious : CardModel { SIM_MODEL(Impervious, CardModel)
bool GainsBlock() const { return true; }
void OnPlay(PlayerChoiceContext&, const CardPlay* p) const { CreatureCmd::GainBlock(Owner().Creature(),DynamicVars().Block(),p); }
};
}
