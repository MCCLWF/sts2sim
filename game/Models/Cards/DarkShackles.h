#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct DarkShackles : CardModel {
    SIM_MODEL(DarkShackles, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("DarkShackles target is null");
        PowerCmd::Apply(PowerId::DARK_SHACKLES_POWER, ctx, play->Target,
                        DynamicVars()["StrengthLoss"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
