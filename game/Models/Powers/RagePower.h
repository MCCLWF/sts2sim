#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct RagePower : PowerModel { SIM_MODEL(RagePower, PowerModel)
void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const { if(play&&play->Card.Owner()==Owner().Player()&&play->Card.Type()==CardType::Attack) CreatureCmd::GainBlock(Owner(),Dec(Amount()),ValueProp::Unpowered,nullptr); }
void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const { if(participants.contains(Owner())) PowerCmd::Remove(*this); }
}; }
