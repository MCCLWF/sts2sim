// Models/Cards/Pillage.cs
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Pillage : CardModel { SIM_MODEL(Pillage, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).Targeting(p->Target).Execute(ctx); while(CardPile::Get(PileType::Hand,Owner()).Count()<CardPile::MaxCardsInHand){ CardModel c=CardPileCmd::Draw(ctx,Owner()); if(!c || c.Type()!=CardType::Attack) break; } }
};
}
