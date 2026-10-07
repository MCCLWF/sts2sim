// Models/Cards/Expertise.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Expertise : CardModel {
    SIM_MODEL(Expertise, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        const int32_t cardCount = DynamicVars().Cards().IntValue();
        CardList drawn = CardPileCmd::Draw(ctx, Dec(cardCount), Owner());
        for (CardModel card : drawn) CardCmd::ApplySingleTurnRetain(card);
    }
};

}  // namespace sts2
