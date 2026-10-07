// Models/Relics/PreciseScissors.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardPileCmd.h"
namespace sts2 {
struct PreciseScissors : RelicModel {
    SIM_MODEL(PreciseScissors, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { CardList cards = CardSelectCmd::FromDeckForRemoval(Owner(), CardSelectorPrefs(1)); CardPileCmd::RemoveFromDeck(cards); }
};
}
