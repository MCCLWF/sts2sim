// Models/Cards/Prepared.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Prepared : CardModel {
    SIM_MODEL(Prepared, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        int32_t cardCount = DynamicVars().Cards().IntValue();
        CardPileCmd::Draw(ctx, Dec(cardCount), Owner());
        CardList selected = CardSelectCmd::FromHandForDiscard(
            ctx, Owner(), CardSelectorPrefs(cardCount), nullptr, *this);
        CardCmd::Discard(ctx, selected);
    }
};

}  // namespace sts2
