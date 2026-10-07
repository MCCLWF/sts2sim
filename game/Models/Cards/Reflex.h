// Models/Cards/Reflex.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Reflex : CardModel {
    SIM_MODEL(Reflex, CardModel)

    void OnPlay(PlayerChoiceContext& context, const CardPlay*) const {
        CardPileCmd::Draw(context, DynamicVars().Cards().BaseValue(), Owner());
    }
};

}  // namespace sts2
