#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Glimmer : CardModel {
    SIM_MODEL(Glimmer, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
        CardList selection = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(DynamicVars()["PutBack"].IntValue()), nullptr, *this);
        if (!selection.empty()) CardPileCmd::Add(selection, PileType::Draw, CardPilePosition::Top);
    }
};
}  // namespace sts2
