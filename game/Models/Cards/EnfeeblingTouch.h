#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct EnfeeblingTouch : CardModel {
    SIM_MODEL(EnfeeblingTouch, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        PowerCmd::Apply(PowerId::ENFEEBLING_TOUCH_POWER, ctx, play->Target,
                        DynamicVars()["StrengthLoss"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
