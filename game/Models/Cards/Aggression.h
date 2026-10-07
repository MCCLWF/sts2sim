// Models/Cards/Aggression.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Aggression : CardModel { SIM_MODEL(Aggression, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::AGGRESSION_POWER,ctx,Owner().Creature(),Dec(1),Owner().Creature(),*this); }
};
}
