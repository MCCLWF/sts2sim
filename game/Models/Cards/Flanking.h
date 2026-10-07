#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Flanking : CardModel {
    SIM_MODEL(Flanking, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        PowerCmd::Apply(PowerId::FLANKING_POWER, ctx, play->Target, Dec(2), Owner().Creature(), *this);
    }
};

}  // namespace sts2
