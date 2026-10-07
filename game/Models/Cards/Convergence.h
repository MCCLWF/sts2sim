#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Convergence : CardModel {
    SIM_MODEL(Convergence, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::RETAIN_HAND_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::ENERGY_NEXT_TURN_POWER, ctx, Owner().Creature(), DynamicVars().Energy().BaseValue(), Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::STAR_NEXT_TURN_POWER, ctx, Owner().Creature(), DynamicVars().Stars().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
