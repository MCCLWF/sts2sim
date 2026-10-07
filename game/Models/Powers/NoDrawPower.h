#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct NoDrawPower : PowerModel { SIM_MODEL(NoDrawPower, PowerModel)
bool ShouldDraw(Player player, bool fromHandDraw) const { return fromHandDraw||player!=Owner().Player(); }
void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const { if(participants.contains(Owner())) PowerCmd::Remove(*this); }
}; }
