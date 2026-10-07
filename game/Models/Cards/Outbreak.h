#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
#include "game/Models/Powers/PoisonPower.h"

namespace sts2 {

struct Outbreak : CardModel {
    SIM_MODEL(Outbreak, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CreatureList enemies = CombatState().HittableEnemies();
        for (Creature enemy : enemies)
            PowerCmd::Apply(PowerId::POISON_POWER, ctx, enemy, DynamicVars().Poison().BaseValue(), Owner().Creature(), *this);
        for (Creature enemy : CombatState().HittableEnemies()) {
            PowerModel poison = enemy.GetPower(PowerId::POISON_POWER);
            if (poison) PoisonPower(poison).Trigger(ctx);
        }
    }
};

}  // namespace sts2
