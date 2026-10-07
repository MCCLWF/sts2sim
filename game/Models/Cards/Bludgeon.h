// Models/Cards/Bludgeon.cs
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Bludgeon : CardModel { SIM_MODEL(Bludgeon, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if (!p||!p->Target) throw GameError("cardPlay.Target is null"); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).Targeting(p->Target).Execute(ctx); }
};
}
