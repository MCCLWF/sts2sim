#pragma once

#include "game/All.h"
#include "game/Commands/DamageCmd.h"

namespace sts2 {

struct GiantRock : CardModel {
    SIM_MODEL(GiantRock, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }

};

}  // namespace sts2
