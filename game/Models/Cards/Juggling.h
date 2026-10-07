// Models/Cards/Juggling.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Juggling : CardModel { SIM_MODEL(Juggling, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::JUGGLING_POWER,ctx,Owner().Creature(),Dec(1),Owner().Creature(),*this); }
};
}
