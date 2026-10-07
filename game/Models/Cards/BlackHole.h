#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct BlackHole : CardModel {
    SIM_MODEL(BlackHole, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::BLACK_HOLE_POWER, ctx, Owner().Creature(), DynamicVars()["BlackHolePower"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
