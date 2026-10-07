#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct RollingBoulder : CardModel {
    SIM_MODEL(RollingBoulder, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ROLLING_BOULDER_POWER, ctx, Owner().Creature(), DynamicVars()["RollingBoulderPower"].BaseValue(),
                        Owner().Creature(), *this);
    }

};

}  // namespace sts2
