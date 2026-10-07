// Models/Relics/NewLeaf.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
namespace sts2 {
struct NewLeaf : RelicModel {
    SIM_MODEL(NewLeaf, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const {
        CardList cards = CardSelectCmd::FromDeckForTransformation(Owner(), CardSelectorPrefs(1));
        for (CardModel c : cards) CardCmd::TransformToRandom(c, Owner().RunState().Rng().Niche());
    }
};
}
