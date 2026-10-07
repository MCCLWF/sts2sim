// Models/Cards/Alignment.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Alignment : CardModel {
    SIM_MODEL(Alignment, CardModel)

    int32_t CanonicalStarCost() const { return 2; }
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        PlayerCmd::GainEnergy(DynamicVars().Energy().IntValue(), Owner());
    }
};

}  // namespace sts2
