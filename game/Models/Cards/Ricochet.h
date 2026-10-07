#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Ricochet : CardModel {
    SIM_MODEL(Ricochet, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .WithHitCount(DynamicVars().Repeat().IntValue()).FromCard(*this, play)
            .TargetingRandomOpponents(CombatState()).Execute(ctx);
    }
};

}  // namespace sts2
