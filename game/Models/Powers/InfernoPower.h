#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct InfernoPower : PowerModel { SIM_MODEL(InfernoPower, PowerModel)
void AfterPlayerTurnStart(PlayerChoiceContext& ctx, Player player) const { if(player==Owner().Player()) CreatureCmd::Damage(ctx,Owner(),DynamicVars()["SelfDamage"].BaseValue(),ValueProp::Unblockable|ValueProp::Unpowered,Owner()); }
void AfterDamageReceived(PlayerChoiceContext& ctx, Creature target, const DamageResult& result, ValueProp, Creature, CardModel) const { if(target==Owner()&&result.UnblockedDamage>0&&Owner().CombatState().CurrentSide()==Owner().Side()) CreatureCmd::Damage(ctx,Owner().CombatState().HittableEnemies(),Dec(Amount()),ValueProp::Unpowered,Owner()); }
void IncrementSelfDamage() const { DynamicVars()["SelfDamage"].SetBaseValue(DynamicVars()["SelfDamage"].BaseValue()+Dec(1)); }
}; }
