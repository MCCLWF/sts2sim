#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct PhantomBlades : CardModel {
    SIM_MODEL(PhantomBlades, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::PHANTOM_BLADES_POWER, ctx, Owner().Creature(),
                        DynamicVars()["PhantomBladesPower"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
