// Models/Cards/StrikeIronclad.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct StrikeIronclad : CardModel {
    SIM_MODEL(StrikeIronclad, CardModel)

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        if (!cardPlay->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, cardPlay).Targeting(cardPlay->Target).Execute(choiceContext);
    }
};

}  // namespace sts2
