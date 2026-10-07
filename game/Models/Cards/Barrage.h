// Models/Cards/Barrage.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Barrage : CardModel { SIM_MODEL(Barrage, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        int32_t hits = DynamicVars()["CalculatedHits"].Calculate(play->Target).to_int();
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(hits)
            .FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};
}
