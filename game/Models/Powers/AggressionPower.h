#pragma once
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct AggressionPower : PowerModel { SIM_MODEL(AggressionPower, PowerModel)
void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, sts2::CombatState) const { if (!participants.contains(Owner())) return; CardList cards; for (CardModel c : CardPile::Get(PileType::Discard, Owner().Player()).Cards()) if (c.Type()==CardType::Attack) cards.push_back(c); Owner().Player().RunState().Rng().CombatCardSelection().Shuffle(cards); int n=std::min(Amount(),(int)cards.size()); for(int i=0;i<n;++i){ CardPileCmd::Add(cards[i],PileType::Hand); if(cards[i].IsUpgradable()) CardCmd::Upgrade(cards[i]); } }
}; }
