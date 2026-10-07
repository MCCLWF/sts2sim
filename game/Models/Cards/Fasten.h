#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Fasten : CardModel {
    SIM_MODEL(Fasten, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::FASTEN_POWER, ctx, Owner().Creature(), DynamicVars()["ExtraBlock"].BaseValue(),
                        Owner().Creature(), *this);
    }
};

}  // namespace sts2
