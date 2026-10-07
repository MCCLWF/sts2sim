#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Envenom : CardModel {
    SIM_MODEL(Envenom, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ENVENOM_POWER, ctx, Owner().Creature(), DynamicVars()["EnvenomPower"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
