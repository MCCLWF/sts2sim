#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Tingsha : RelicModel {
    SIM_MODEL(Tingsha, RelicModel)

    void AfterCardDiscarded(PlayerChoiceContext& ctx, CardModel card) const {
        if (card.Owner() != Owner() || Owner().Creature().Side() != Owner().Creature().CombatState().CurrentSide()) return;
        CreatureList enemies = Owner().Creature().CombatState().HittableEnemies();
        int32_t index = Owner().RunState().Rng().CombatTargets().next_item_index(static_cast<int32_t>(enemies.size()));
        if (index >= 0)
            CreatureCmd::Damage(ctx, enemies[static_cast<size_t>(index)], DynamicVars().Damage(), Owner().Creature());
    }
};

}  // namespace sts2
