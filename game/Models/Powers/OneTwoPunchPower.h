#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct OneTwoPunchPower : PowerModel { SIM_MODEL(OneTwoPunchPower, PowerModel)
int32_t ModifyCardPlayCount(CardModel card, Creature, int32_t count) const { if(card.Owner().Creature()==Owner()&&card.Type()==CardType::Attack) return count+1; return count; }
void AfterModifyingCardPlayCount(CardModel) const { PowerCmd::Decrement(*this); }
void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const { if(participants.contains(Owner())) PowerCmd::Remove(*this); }
}; }
