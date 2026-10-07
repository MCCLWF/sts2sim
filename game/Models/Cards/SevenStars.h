#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct SevenStars : CardModel {
    SIM_MODEL(SevenStars, CardModel)
    int32_t CanonicalStarCost() const { return 7; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(DynamicVars().Repeat().IntValue()).FromCard(*this, play).TargetingAllOpponents(CombatState()).Execute(ctx);
    }
};
}  // namespace sts2
