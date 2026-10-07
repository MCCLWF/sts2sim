// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct ShrugItOff : CardModel { SIM_MODEL(ShrugItOff, CardModel)
 bool GainsBlock() const { return true; }
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const { CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play); CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner()); }
};
}
