// Models/Cards/Havoc.cs
#include "game/Commands/CardPileCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct Havoc : CardModel { SIM_MODEL(Havoc, CardModel)
 void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { CardPileCmd::AutoPlayFromDrawPile(ctx, Owner(), 1, CardPilePosition::Top, true); }
};
}
