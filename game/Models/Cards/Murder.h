#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Murder : CardModel {
    SIM_MODEL(Murder, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("Murder: cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().CalculatedDamage().Calculate(play->Target)).FromCard(*this, play)
            .Targeting(play->Target).Execute(ctx);
    }
};

}  // namespace sts2
