#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct PrepTime : CardModel {
    SIM_MODEL(PrepTime, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::PREP_TIME_POWER, ctx, Owner().Creature(), DynamicVars()["PrepTimePower"].BaseValue(),
                        Owner().Creature(), *this);
    }

};

}  // namespace sts2
