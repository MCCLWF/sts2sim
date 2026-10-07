// Models/Cards/Bully.cs
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Bully : CardModel { SIM_MODEL(Bully, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if (!p||!p->Target) throw GameError("cardPlay.Target is null"); DamageCmd::Attack(DynamicVars().CalculatedDamage()).FromCard(*this,p).Targeting(p->Target).Execute(ctx); }
};
}
