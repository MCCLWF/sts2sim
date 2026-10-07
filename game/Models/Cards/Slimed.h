// Models/Cards/Slimed.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Slimed : CardModel {
    SIM_MODEL(Slimed, CardModel)

    int32_t MaxUpgradeLevel() const { return 0; }
    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        CardPileCmd::Draw(choiceContext, DynamicVars().Cards().BaseValue(), Owner());
    }
};

}  // namespace sts2
