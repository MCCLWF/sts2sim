#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Tactician : CardModel {
    SIM_MODEL(Tactician, CardModel)

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        PlayerCmd::GainEnergy(DynamicVars().Energy().IntValue(), Owner());
    }
};

}  // namespace sts2
