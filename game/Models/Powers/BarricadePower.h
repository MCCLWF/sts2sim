#pragma once
#include "game/Models/PowerModel.h"
namespace sts2 {
struct BarricadePower : PowerModel { SIM_MODEL(BarricadePower, PowerModel)
void AfterApplied(Creature, CardModel) const {}
bool ShouldClearBlock(Creature creature) const { return creature != Owner(); }
}; }
