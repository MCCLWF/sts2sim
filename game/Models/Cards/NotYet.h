// Models/Cards/NotYet.cs
#include "game/Commands/CreatureCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct NotYet : CardModel { SIM_MODEL(NotYet, CardModel)
bool CanBeGeneratedInCombat() const { return false; }
void OnPlay(PlayerChoiceContext&, const CardPlay*) const { CreatureCmd::Heal(Owner().Creature(),DynamicVars().Heal().BaseValue()); }
};
}
