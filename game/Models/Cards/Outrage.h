// Models/Cards/Outrage.cs
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Outrage : CardModel { SIM_MODEL(Outrage, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { if(!p||!p->Target) throw GameError("cardPlay.Target is null"); DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this,p).Targeting(p->Target).Execute(ctx); CardPileCmd::AddGeneratedCardToCombat(CreateClone(),PileType::Discard,Owner()); }
};
}
