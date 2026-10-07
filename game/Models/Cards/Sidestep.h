// Models/Cards/Sidestep.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Sidestep : CardModel {
    SIM_MODEL(Sidestep, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ENERGY_NEXT_TURN_POWER, ctx, Owner().Creature(),
                        Dec(DynamicVars().Energy().IntValue()), Owner().Creature(), *this);
    }
};

}  // namespace sts2
