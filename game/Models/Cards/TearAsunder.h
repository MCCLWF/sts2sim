// Models/Cards/TearAsunder.cs
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct TearAsunder : CardModel { SIM_MODEL(TearAsunder, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); int32_t hits=DynamicVars()["CalculatedHits"].Calculate(p->Target).to_int(); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(hits).FromCard(*this,p).Targeting(p->Target).Execute(ctx); }
};
}
