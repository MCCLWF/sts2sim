// Models/Relics/Astrolabe.cs.
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Astrolabe : RelicModel {
    SIM_MODEL(Astrolabe, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardList selected = CardSelectCmd::FromDeckForTransformation(Owner(), CardSelectorPrefs(DynamicVars().Cards().IntValue()));
        for (CardModel card : selected) {
            CardModel replacement = CardFactory::CreateRandomCardForTransform(card, /*isInCombat=*/false, Owner().RunState().Rng().Niche());
            CardCmd::Upgrade(replacement);
            CardCmd::Transform(card, replacement);
        }
    }
};

}  // namespace sts2
