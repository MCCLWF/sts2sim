// Models/Cards/FiendFire.cs
#include "game/Commands/CardCmd.h"
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct FiendFire : CardModel { SIM_MODEL(FiendFire, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); CardList cards=CardPile::Get(PileType::Hand,Owner()).Cards(); int32_t n=static_cast<int32_t>(cards.size()); for(CardModel c:cards) CardCmd::Exhaust(ctx,c); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(n).FromCard(*this,p).Targeting(p->Target).Execute(ctx); }
};
}
