// Models/Cards/RoyalGamble.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct RoyalGamble : CardModel {
    SIM_MODEL(RoyalGamble, CardModel)

    int32_t CanonicalStarCost() const { return 5; }
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
    }
};

}  // namespace sts2
