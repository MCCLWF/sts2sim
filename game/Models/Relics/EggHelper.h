// Helpers/Models/EggRelicHelper.cs: reward and merchant results are represented as CardList.
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2::egg_helper {

inline CardModel UpgradeClone(CardModel card, RelicModel egg) {
    CardModel clone = egg.Owner().RunState().CloneCard(card);
    CardCmd::Upgrade(clone);
    return clone;
}

inline void UpgradeValidCards(CardList& cards, CardType type, RelicModel egg) {
    for (CardModel& card : cards)
        if (card.Type() == type && card.IsUpgradable()) card = UpgradeClone(card, egg);
}

inline bool TryModifyAddedCard(CardModel card, CardType type, RelicModel egg, CardModel& newCard,
                               bool onlyUnupgraded = false) {
    newCard = {};
    if (card.Owner() != egg.Owner() || card.Type() != type || !card.IsUpgradable() ||
        (onlyUnupgraded && card.CurrentUpgradeLevel() >= 1)) return false;
    newCard = UpgradeClone(card, egg);
    return true;
}

}  // namespace sts2::egg_helper
