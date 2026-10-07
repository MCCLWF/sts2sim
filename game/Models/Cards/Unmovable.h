// Models/Cards/Unmovable.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Unmovable : CardModel { SIM_MODEL(Unmovable, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::UNMOVABLE_POWER,ctx,Owner().Creature(),Dec(1),Owner().Creature(),*this); }
};
}
