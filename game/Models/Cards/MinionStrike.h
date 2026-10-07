// Models/Cards/MinionStrike.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct MinionStrike : CardModel {
    SIM_MODEL(MinionStrike, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("MinionStrike: cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
    }
};
}  // namespace sts2
