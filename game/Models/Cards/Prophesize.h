// Models/Cards/Prophesize.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Prophesize : CardModel {
    SIM_MODEL(Prophesize, CardModel)

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay*) const {
        CardPileCmd::Draw(choiceContext, DynamicVars().Cards().BaseValue(), Owner());
    }
};

}  // namespace sts2
