#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Finisher : CardModel {
    SIM_MODEL(Finisher, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("Finisher: cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .WithHitCount(DynamicVars()["CalculatedHits"].Calculate(play->Target).to_int())
            .FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};

}  // namespace sts2
