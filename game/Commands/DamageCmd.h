// Commands/DamageCmd.cs
#pragma once

#include "game/Commands/Builders/AttackCommand.h"

namespace sts2::DamageCmd {

inline AttackCommand Attack(Dec damagePerHit) { return AttackCommand(damagePerHit); }
inline AttackCommand Attack(int32_t damagePerHit) { return AttackCommand(Dec(damagePerHit)); }
inline AttackCommand Attack(DynamicVar calculatedDamageVar) { return AttackCommand(calculatedDamageVar); }

}  // namespace sts2::DamageCmd
