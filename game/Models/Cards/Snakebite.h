#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Snakebite : CardModel {
    SIM_MODEL(Snakebite, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("Snakebite: cardPlay.Target is null");
        PowerCmd::Apply(PowerId::POISON_POWER, ctx, play->Target, DynamicVars().Poison().BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
