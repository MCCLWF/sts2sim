#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct ViciousPower : PowerModel { SIM_MODEL(ViciousPower, PowerModel)
void AfterPowerAmountChanged(PlayerChoiceContext& ctx, PowerModel power, Dec amount, Creature applier, CardModel) const { if(amount>Dec(0)&&applier==Owner()&&power.PowerId_()==PowerId::VULNERABLE_POWER) CardPileCmd::Draw(ctx,Dec(Amount()),Owner().Player()); }
}; }
