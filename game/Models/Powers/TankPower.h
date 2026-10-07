#pragma once
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"
namespace sts2 {
struct TankPower : PowerModel { SIM_MODEL(TankPower, PowerModel)
void AfterApplied(Creature, CardModel) const {}
Dec ModifyDamageMultiplicative(Creature target, Dec amount, ValueProp props, Creature, CardModel, const CardPlay*) const { return target==Owner()&&IsPoweredAttack(props)?DynamicVars()["DamageIncrease"].BaseValue():Dec(1); }
}; }
