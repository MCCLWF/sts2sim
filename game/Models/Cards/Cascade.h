// Models/Cards/Cascade.cs
#include "game/Commands/CardPileCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct Cascade : CardModel { SIM_MODEL(Cascade, CardModel)
bool HasEnergyCostX() const { return true; }
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { CardPileCmd::AutoPlayFromDrawPile(ctx,Owner(),ResolveEnergyXValue()+(CurrentUpgradeLevel()>0?1:0)); }
};
}
