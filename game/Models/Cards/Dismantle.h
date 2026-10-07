// Models/Cards/Dismantle.cs
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Dismantle : CardModel { SIM_MODEL(Dismantle, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); int32_t hits=p->Target.HasPower(PowerId::VULNERABLE_POWER)?2:1; DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(hits).FromCard(*this,p).Targeting(p->Target).Execute(ctx); }
};
}
