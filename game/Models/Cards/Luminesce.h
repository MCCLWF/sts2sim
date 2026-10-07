#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Luminesce : CardModel {
    SIM_MODEL(Luminesce, CardModel)

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }

};

}  // namespace sts2
