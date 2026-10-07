// Models/Relics/PreservedFog.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PreservedFog : RelicModel {
    SIM_MODEL(PreservedFog, RelicModel)

    void AfterObtained() const {
        CardList selected = CardSelectCmd::FromDeckForRemoval(Owner(), CardSelectorPrefs(DynamicVars().Cards().IntValue()));
        for (CardModel card : selected) CardPileCmd::RemoveFromDeck(card);
        CardPileCmd::AddCurseToDeck(CardId::FOLLY, Owner());
    }
};

}  // namespace sts2
