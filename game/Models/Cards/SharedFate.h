// Models/Cards/SharedFate.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct SharedFate : CardModel {
    SIM_MODEL(SharedFate, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner().Creature(),
                        -DynamicVars()["PlayerStrengthLoss"].BaseValue(), Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, play->Target,
                        -DynamicVars()["EnemyStrengthLoss"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
