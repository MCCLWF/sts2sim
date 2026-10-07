#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Afterimage : CardModel {
    SIM_MODEL(Afterimage, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::AFTERIMAGE_POWER, ctx, Owner().Creature(), DynamicVars()["AfterimagePower"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
