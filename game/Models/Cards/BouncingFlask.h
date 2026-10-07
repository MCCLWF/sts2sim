#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct BouncingFlask : CardModel {
    SIM_MODEL(BouncingFlask, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        for (int32_t i = 0; i < DynamicVars().Repeat().IntValue(); ++i) {
            CreatureList enemies = CombatState().HittableEnemies();
            int32_t index = Owner().RunState().Rng().CombatTargets().next_item_index(static_cast<int32_t>(enemies.size()));
            if (index < 0) continue;
            Creature enemy = enemies[static_cast<size_t>(index)];
            PowerCmd::Apply(PowerId::POISON_POWER, ctx, enemy, DynamicVars().Poison().BaseValue(), Owner().Creature(), *this);
        }
    }
};

}  // namespace sts2
