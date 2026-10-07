// Models/Cards/DarkEmbrace.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct DarkEmbrace : CardModel { SIM_MODEL(DarkEmbrace, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::DARK_EMBRACE_POWER,ctx,Owner().Creature(),Dec(1),Owner().Creature(),*this); }
};
}
