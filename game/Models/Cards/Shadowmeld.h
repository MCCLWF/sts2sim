#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Shadowmeld : CardModel {
    SIM_MODEL(Shadowmeld, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::SHADOWMELD_POWER, ctx, Owner().Creature(),
                        DynamicVars()["Power"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
