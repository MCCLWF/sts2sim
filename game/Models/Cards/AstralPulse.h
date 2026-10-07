// Models/Cards/AstralPulse.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct AstralPulse : CardModel {
    SIM_MODEL(AstralPulse, CardModel)

    int32_t CanonicalStarCost() const { return 3; }
    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .WithHitCount(2)
            .FromCard(*this, cardPlay)
            .TargetingAllOpponents(CombatState())
            .Execute(choiceContext);
    }
};

}  // namespace sts2
