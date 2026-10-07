// Models/Relics/EmptyCage.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct EmptyCage : RelicModel {
    SIM_MODEL(EmptyCage, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardList selected = CardSelectCmd::FromDeckForRemoval(Owner(), CardSelectorPrefs(DynamicVars().Cards().IntValue()));
        CardPileCmd::RemoveFromDeck(selected);
    }
};

}  // namespace sts2
