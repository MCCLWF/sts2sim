#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/Entities/Creatures/DamageResult.h"
#include "game/ValueProps/ValuePropExtensions.h"
namespace sts2 {
struct FlameBarrierPower : PowerModel { SIM_MODEL(FlameBarrierPower, PowerModel)
void AfterDamageReceived(PlayerChoiceContext& ctx, Creature target, const DamageResult&, ValueProp props, Creature dealer, CardModel) const { if(target==Owner()&&dealer&&IsPoweredAttack(props)) CreatureCmd::Damage(ctx,dealer,Dec(Amount()),ValueProp::Unpowered,Owner()); }
void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide side, const CreatureList&) const { if(Owner().Side()!=side) PowerCmd::Remove(*this); }
}; }
