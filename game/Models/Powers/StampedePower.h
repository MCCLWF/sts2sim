#pragma once
#include "game/Commands/CardCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct StampedePower : PowerModel { SIM_MODEL(StampedePower, PowerModel)
void AfterAutoPostPlayPhaseEntered(PlayerChoiceContext& ctx, Player player) const { if(player!=Owner().Player()) return; for(int i=0;i<Amount();++i){ CardList options; for(CardModel c:CardPile::Get(PileType::Hand,player).Cards()) if(c.Type()==CardType::Attack&&!c.Keywords().Contains(CardKeyword::Unplayable)) options.push_back(c); if(options.empty()) break; CardCmd::AutoPlay(ctx,player.RunState().Rng().Shuffle().NextItem(options),Creature{}); } }
}; }
