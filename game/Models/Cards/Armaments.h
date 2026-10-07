// Models/Cards/%s.cs
#pragma once

#include "game/All.h"

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Armaments : CardModel { SIM_MODEL(Armaments, CardModel)
 bool GainsBlock() const { return true; }
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
  CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
  if (CurrentUpgradeLevel() > 0) { CardCmd::Upgrade(Owner().PlayerCombatState().Hand().Cards()); return; }
  CardModel card = CardSelectCmd::FromHandForUpgrade(ctx, Owner(), *this); if (card) CardCmd::Upgrade(card);
 }
};
}
