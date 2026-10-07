// Models/Relics/PandorasBox.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PandorasBox : RelicModel {
    SIM_MODEL(PandorasBox, RelicModel)

    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const {
        std::vector<CardTransformation> transformations;
        for (CardModel card : Owner().Deck().Cards())
            if (card && card.IsBasicStrikeOrDefend() && card.IsRemovable()) transformations.emplace_back(card);
        CardCmd::Transform(transformations, &Owner().RunState().Rng().Niche());
    }
};

}  // namespace sts2
