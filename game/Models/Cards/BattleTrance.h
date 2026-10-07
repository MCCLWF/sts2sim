// Models/Cards/BattleTrance.cs
#include "game/Commands/PowerCmd.h"
#include "game/Commands/CardPileCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct BattleTrance : CardModel { SIM_MODEL(BattleTrance, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { CardPileCmd::Draw(ctx,DynamicVars().Cards().BaseValue(),Owner()); PowerCmd::Apply(PowerId::NO_DRAW_POWER,ctx,Owner().Creature(),Dec(1),Owner().Creature(),*this); }
};
}
