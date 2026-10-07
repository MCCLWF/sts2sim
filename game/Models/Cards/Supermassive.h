// Models/Cards/Supermassive.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Supermassive : CardModel {
    SIM_MODEL(Supermassive, CardModel)

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        if (!cardPlay || !cardPlay->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().CalculatedDamage())
            .FromCard(*this, cardPlay)
            .Targeting(cardPlay->Target)
            .Execute(choiceContext);
    }
};

}  // namespace sts2
