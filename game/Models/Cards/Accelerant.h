#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Accelerant : CardModel {
    SIM_MODEL(Accelerant, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ACCELERANT_POWER, ctx, Owner().Creature(), DynamicVars()["Accelerant"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
