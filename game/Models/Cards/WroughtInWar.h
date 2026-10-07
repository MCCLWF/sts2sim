#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/ForgeCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct WroughtInWar : CardModel {
    SIM_MODEL(WroughtInWar, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        ForgeCmd::Forge(DynamicVars().Forge().IntValue(), Owner(), *this);
    }
};
}  // namespace sts2
