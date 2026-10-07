#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct CelestialMight : CardModel {
    SIM_MODEL(CelestialMight, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(DynamicVars().Repeat().IntValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};
}  // namespace sts2
