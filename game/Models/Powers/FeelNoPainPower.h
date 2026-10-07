#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct FeelNoPainPower : PowerModel { SIM_MODEL(FeelNoPainPower, PowerModel)
void AfterCardExhausted(PlayerChoiceContext&, CardModel card, bool) const { if(card.Owner().Creature()==Owner()) CreatureCmd::GainBlock(Owner(),Dec(Amount()),ValueProp::Unpowered,nullptr); }
}; }
