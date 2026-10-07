#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Caltrops : CardModel {
    SIM_MODEL(Caltrops, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::THORNS_POWER, ctx, Owner().Creature(), DynamicVars()["ThornsPower"].BaseValue(),
                        Owner().Creature(), *this);
    }

};

}  // namespace sts2
