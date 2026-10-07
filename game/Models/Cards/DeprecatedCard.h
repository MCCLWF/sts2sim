#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct DeprecatedCard : CardModel {
    SIM_MODEL(DeprecatedCard, CardModel)

    int32_t MaxUpgradeLevel() const { return 0; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardPileCmd::Draw(ctx, Dec(1), Owner());
        CardModel deckVersion = DeckVersion();
        if (deckVersion) {
            CardPileCmd::RemoveFromDeck(deckVersion);
            st().deck_version = 0;
        }
    }

};

}  // namespace sts2
