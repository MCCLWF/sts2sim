#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Sneaky : CardModel {
    SIM_MODEL(Sneaky, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::SNEAKY_POWER, ctx, Owner().Creature(), DynamicVars()["SneakyPower"].BaseValue(),
                        Owner().Creature(), *this);
    }
};

}  // namespace sts2
