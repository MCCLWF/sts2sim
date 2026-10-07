// Models/Cards/FlameBarrier.cs
#include "game/Commands/PowerCmd.h"
#include "game/Commands/CreatureCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct FlameBarrier : CardModel { SIM_MODEL(FlameBarrier, CardModel)
bool GainsBlock() const { return true; }
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { CreatureCmd::GainBlock(Owner().Creature(),DynamicVars().Block(),p); PowerCmd::Apply(PowerId::FLAME_BARRIER_POWER,ctx,Owner().Creature(),DynamicVars()["DamageBack"].BaseValue(),Owner().Creature(),*this); }
};
}
