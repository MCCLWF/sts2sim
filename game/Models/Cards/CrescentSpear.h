#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct CrescentSpear : CardModel {
    SIM_MODEL(CrescentSpear, CardModel)
    int32_t CanonicalStarCost() const { return 1; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().CalculatedDamage()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};
}  // namespace sts2
