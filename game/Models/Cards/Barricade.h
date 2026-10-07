// Models/Cards/Barricade.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Barricade : CardModel { SIM_MODEL(Barricade, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::BARRICADE_POWER,ctx,Owner().Creature(),Dec(1),Owner().Creature(),*this); }
};
}
