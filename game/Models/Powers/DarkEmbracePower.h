#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct DarkEmbracePower : PowerModel { SIM_MODEL(DarkEmbracePower, PowerModel)
void AfterCardExhausted(PlayerChoiceContext& ctx, CardModel card, bool causedByEthereal) const { if(card.Owner().Creature()!=Owner()) return; if(!causedByEthereal) CardPileCmd::Draw(ctx,Dec(Amount()),Owner().Player()); else extra(0)++; }
void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const { if(participants.contains(Owner()) && extra(0)){ CardPileCmd::Draw(ctx,Dec(Amount()*extra(0)),Owner().Player()); extra(0)=0; } }
}; }
