#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Haze : CardModel {
    SIM_MODEL(Haze, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::POISON_POWER, ctx, CombatState().HittableEnemies(), DynamicVars().Poison().BaseValue(),
                        Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, CombatState().HittableEnemies(), DynamicVars().Weak().BaseValue(),
                        Owner().Creature(), *this);
    }
};

}  // namespace sts2
