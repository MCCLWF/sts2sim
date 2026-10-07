#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Burst : CardModel {
    SIM_MODEL(Burst, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::BURST_POWER, ctx, Owner().Creature(), DynamicVars()["Skills"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
