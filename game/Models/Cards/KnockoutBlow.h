// Models/Cards/KnockoutBlow.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct KnockoutBlow : CardModel {
    SIM_MODEL(KnockoutBlow, CardModel)

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        if (!cardPlay || !cardPlay->Target) throw GameError("cardPlay.Target is null");
        AttackCommand attack = DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .FromCard(*this, cardPlay)
            .Targeting(cardPlay->Target)
            .Execute(choiceContext);
        for (const auto& hit : attack.Results()) {
            for (const DamageResult& result : hit) {
                if (result.WasTargetKilled) {
                    PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
                    return;
                }
            }
        }
    }
};

}  // namespace sts2
