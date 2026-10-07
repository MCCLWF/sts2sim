#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct DemonFormPower : PowerModel { SIM_MODEL(DemonFormPower, PowerModel)
void AfterApplied(Creature, CardModel) const {}
void AfterRemoved(Creature) const {}
void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const { if(participants.contains(Owner())) { PlayerChoiceContext ctx=PlayerChoiceContext::Throwing(); PowerCmd::Apply(PowerId::STRENGTH_POWER,ctx,Owner(),Dec(Amount()),Owner(),CardModel{}); } }
}; }
