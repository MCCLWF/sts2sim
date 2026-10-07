// Models/Relics/BingBong.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BingBong : RelicModel {
    SIM_MODEL(BingBong, RelicModel)

    void AfterCardChangedPiles(CardModel card, PileType, AbstractModel clonedBy) const {
        CardPile pile = card.Pile();
        if (card.Owner() != Owner() || !pile || pile.Type() != PileType::Deck || clonedBy) return;
        if (card.st().bing_bong_skip_relic == Index()) {
            card.st().bing_bong_skip_relic = 0;  // CardsToSkip.Remove(card).
            return;
        }
        CardModel copy = Owner().RunState().CloneCard(card);
        copy.st().bing_bong_skip_relic = Index();  // CardsToSkip.Add(cardModel).
        CardPileCmd::Add(copy, PileType::Deck, CardPilePosition::Bottom, *this);
    }
};

}  // namespace sts2
