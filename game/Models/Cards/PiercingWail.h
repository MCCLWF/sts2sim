#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct PiercingWail : CardModel {
    SIM_MODEL(PiercingWail, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        for (Creature enemy : CombatState().HittableEnemies())
            PowerCmd::Apply(PowerId::PIERCING_WAIL_POWER, ctx, enemy,
                            DynamicVars()["StrengthLoss"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
