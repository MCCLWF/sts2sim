// Models/Cards/BurningPact.cs
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct BurningPact : CardModel { SIM_MODEL(BurningPact, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { CardList cards=CardSelectCmd::FromHand(ctx,Owner(),CardSelectorPrefs(1),nullptr,*this); if(!cards.empty()) CardCmd::Exhaust(ctx,cards[0]); CardPileCmd::Draw(ctx,DynamicVars().Cards().BaseValue(),Owner()); }
};
}
