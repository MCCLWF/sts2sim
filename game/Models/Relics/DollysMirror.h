// Models/Relics/DollysMirror.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct DollysMirror : RelicModel {
    SIM_MODEL(DollysMirror, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardList selected = CardSelectCmd::FromDeckGeneric(Owner(), CardSelectorPrefs(1), notQuest);
        if (selected.empty()) return;
        CardModel clone = Owner().RunState().CloneCard(selected[0]);
        CardPileCmd::Add(clone, PileType::Deck);
    }

private:
    static bool notQuest(CardModel card) { return card.Type() != CardType::Quest; }
};

}  // namespace sts2
