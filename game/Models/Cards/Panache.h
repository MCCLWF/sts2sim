#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Panache : CardModel {
    SIM_MODEL(Panache, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::PANACHE_POWER, ctx, Owner().Creature(), DynamicVars()["PanacheDamage"].BaseValue(),
                        Owner().Creature(), *this);
    }

};

}  // namespace sts2
