// Models/Cards/SolarStrike.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct SolarStrike : CardModel {
    SIM_MODEL(SolarStrike, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
    }
};
}  // namespace sts2
