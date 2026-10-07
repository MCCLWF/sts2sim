// Models/Cards/Venerate.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Venerate : CardModel {
    SIM_MODEL(Venerate, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
    }
};
}  // namespace sts2
