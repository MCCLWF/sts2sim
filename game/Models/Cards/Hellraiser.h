// Models/Cards/Hellraiser.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Hellraiser : CardModel { SIM_MODEL(Hellraiser, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::HELLRAISER_POWER,ctx,Owner().Creature(),Dec(1),Owner().Creature(),*this); }
};
}
