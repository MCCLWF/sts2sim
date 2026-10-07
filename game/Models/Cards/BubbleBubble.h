#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct BubbleBubble : CardModel {
    SIM_MODEL(BubbleBubble, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        if (!play->Target.HasPower(PowerId::POISON_POWER)) return;
        PowerCmd::Apply(PowerId::POISON_POWER, ctx, play->Target, DynamicVars().Poison().BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
