#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Parry : CardModel {
    SIM_MODEL(Parry, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::PARRY_POWER, ctx, Owner().Creature(), DynamicVars()["ParryPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
