#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct HeavenlyDrill : CardModel {
    SIM_MODEL(HeavenlyDrill, CardModel)
    bool HasEnergyCostX() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        int32_t hits = ResolveEnergyXValue();
        if (hits >= DynamicVars().Energy().IntValue()) hits *= 2;
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(hits).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};
}  // namespace sts2
