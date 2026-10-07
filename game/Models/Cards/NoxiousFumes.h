#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct NoxiousFumes : CardModel {
    SIM_MODEL(NoxiousFumes, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::NOXIOUS_FUMES_POWER, ctx, Owner().Creature(),
                        DynamicVars()["PoisonPerTurn"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
