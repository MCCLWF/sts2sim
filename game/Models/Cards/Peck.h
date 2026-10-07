#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Peck : CardModel {
    SIM_MODEL(Peck, CardModel)

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        if (!cardPlay || !cardPlay->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage()).WithHitCount(DynamicVars().Repeat().BaseValue().to_int())
            .FromCard(*this, cardPlay).Targeting(cardPlay->Target).Execute(choiceContext);
    }

};

}  // namespace sts2
