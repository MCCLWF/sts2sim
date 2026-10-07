// Models/Cards/Flechettes.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Flechettes : CardModel {
    SIM_MODEL(Flechettes, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        int32_t hits = DynamicVars()["CalculatedHits"].Calculate(play->Target).to_int();
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .WithHitCount(hits).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};

}  // namespace sts2
