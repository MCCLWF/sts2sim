#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct ThinkingAhead : CardModel {
    SIM_MODEL(ThinkingAhead, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
        CardList cards = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(1), nullptr, *this);
        if (!cards.empty()) CardPileCmd::Add(cards.front(), PileType::Draw, CardPilePosition::Top);
    }
};

}  // namespace sts2
