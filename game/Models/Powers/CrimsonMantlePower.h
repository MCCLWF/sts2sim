#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct CrimsonMantlePower : PowerModel { SIM_MODEL(CrimsonMantlePower, PowerModel)
void AfterPlayerTurnStart(PlayerChoiceContext& ctx, Player player) const { if(player!=Owner().Player()) return; CreatureCmd::Damage(ctx,Owner(),DynamicVars()["SelfDamage"].BaseValue(),ValueProp::Unblockable|ValueProp::Unpowered,Owner()); CreatureCmd::GainBlock(Owner(),Dec(Amount()),ValueProp::Unpowered,nullptr); }
void IncrementSelfDamage() const { DynamicVars()["SelfDamage"].SetBaseValue(DynamicVars()["SelfDamage"].BaseValue()+Dec(1)); }
}; }
