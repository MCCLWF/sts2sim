#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Tracking : CardModel {
    SIM_MODEL(Tracking, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::TRACKING_POWER, ctx, Owner().Creature(), Dec(50), Owner().Creature(), *this);
    }
};

}  // namespace sts2
