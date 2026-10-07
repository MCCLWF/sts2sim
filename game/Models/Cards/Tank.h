// Models/Cards/Tank.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Tank : CardModel { SIM_MODEL(Tank, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::TANK_POWER,ctx,Owner().Creature(),Dec(1),Owner().Creature(),*this); }
};
}
