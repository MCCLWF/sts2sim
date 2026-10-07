#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"
namespace sts2 {
struct ColossusPower : PowerModel { SIM_MODEL(ColossusPower, PowerModel)
Dec ModifyDamageMultiplicative(Creature target, Dec amount, ValueProp props, Creature dealer, CardModel, const CardPlay*) const { if(target!=Owner()||!IsPoweredAttack(props)||!dealer||!dealer.GetPower(PowerId::VULNERABLE_POWER)) return Dec(1); return DynamicVars()["DamageDecrease"].BaseValue(); }
void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide side, const CreatureList&) const { if(side==CombatSide::Enemy) PowerCmd::Decrement(*this); }
}; }
