#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Stratagem : CardModel {
    SIM_MODEL(Stratagem, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::STRATAGEM_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};

}  // namespace sts2
