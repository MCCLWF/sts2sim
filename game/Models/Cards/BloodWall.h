// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct BloodWall : CardModel { SIM_MODEL(BloodWall, CardModel)
 bool GainsBlock() const { return true; }
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
  CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars().HpLoss().BaseValue(), ValueProp::Unblockable|ValueProp::Unpowered|ValueProp::Move, *this, play);
  CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
 }
};
}
