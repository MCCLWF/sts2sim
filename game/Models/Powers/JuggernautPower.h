#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct JuggernautPower : PowerModel { SIM_MODEL(JuggernautPower, PowerModel)
void AfterBlockGained(Creature creature, Dec amount, ValueProp, CardModel) const { if(creature!=Owner()||amount<=Dec(0)) return; CreatureList enemies=Owner().CombatState().HittableEnemies(); if(!enemies.empty()) { PlayerChoiceContext ctx=PlayerChoiceContext::Throwing(); CreatureCmd::Damage(ctx,Owner().Player().RunState().Rng().CombatTargets().NextItem(enemies),Dec(Amount()),ValueProp::Unpowered,Owner()); } }
}; }
