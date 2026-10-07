// Models/Cards/SecondWind.cs
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/CardCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct SecondWind : CardModel { SIM_MODEL(SecondWind, CardModel)
bool GainsBlock() const { return true; }
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { CardList hand=CardPile::Get(PileType::Hand,Owner()).Cards(); for(CardModel c:hand) if(c.Type()!=CardType::Attack){ CardCmd::Exhaust(ctx,c); CreatureCmd::GainBlock(Owner().Creature(),DynamicVars().Block(),p); } }
};
}
