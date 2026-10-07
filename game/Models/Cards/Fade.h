#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Fade : CardModel {
    SIM_MODEL(Fade, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("Fade: cardPlay.Target is null");
        PowerCmd::Apply(PowerId::FADE_POWER, ctx, play->Target, DynamicVars().Dexterity().BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
