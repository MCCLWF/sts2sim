#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Orbit : CardModel {
    SIM_MODEL(Orbit, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ORBIT_POWER, ctx, Owner().Creature(), DynamicVars().Energy().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
