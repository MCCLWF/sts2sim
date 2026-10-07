#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Skewer : CardModel {
    SIM_MODEL(Skewer, CardModel)

    bool HasEnergyCostX() const { return true; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("Skewer: cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(ResolveEnergyXValue())
            .FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};

}  // namespace sts2
