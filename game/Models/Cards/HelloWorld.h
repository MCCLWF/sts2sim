#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct HelloWorld : CardModel {
    SIM_MODEL(HelloWorld, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::HELLO_WORLD_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }

};

}  // namespace sts2
