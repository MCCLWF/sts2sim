#pragma once
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"
namespace sts2 {
struct CrueltyPower : PowerModel { SIM_MODEL(CrueltyPower, PowerModel)
Dec ModifyVulnerableMultiplier(Creature target, Dec amount, ValueProp props, Creature, CardModel) const { if(target==Owner()||!IsPoweredAttack(props)) return amount; return amount + Dec(Amount())/Dec(100); }
}; }
