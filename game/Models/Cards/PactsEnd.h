// Models/Cards/PactsEnd.cs
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct PactsEnd : CardModel { SIM_MODEL(PactsEnd, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(CardPile::Get(PileType::Exhaust,Owner()).Count()>=DynamicVars().Cards().IntValue()) DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).TargetingAllOpponents(CombatState()).Execute(ctx); }
};
}
