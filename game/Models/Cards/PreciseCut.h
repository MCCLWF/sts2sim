#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct PreciseCut : CardModel {
    SIM_MODEL(PreciseCut, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("PreciseCut: cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().CalculatedDamage().Calculate(play->Target)).FromCard(*this, play)
            .Targeting(play->Target).Execute(ctx);
    }
};

}  // namespace sts2
