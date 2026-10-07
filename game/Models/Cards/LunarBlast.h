// Models/Cards/LunarBlast.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct LunarBlast : CardModel {
    SIM_MODEL(LunarBlast, CardModel)

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        if (!cardPlay || !cardPlay->Target) throw GameError("cardPlay.Target is null");
        int32_t hits = DynamicVars()["CalculatedHits"].Calculate(cardPlay->Target).to_int();
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .WithHitCount(hits)
            .FromCard(*this, cardPlay)
            .Targeting(cardPlay->Target)
            .Execute(choiceContext);
    }
};

}  // namespace sts2
