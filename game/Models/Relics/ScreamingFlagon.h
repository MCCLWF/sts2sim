// Models/Relics/ScreamingFlagon.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct ScreamingFlagon : RelicModel {
    SIM_MODEL(ScreamingFlagon, RelicModel)

    void BeforeSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide, const CreatureList& participants) const {
        Player player = Owner();
        if (!participants.contains(player.Creature()) || player.PlayerCombatState().Hand().Count() != 0) return;
        CreatureList enemies = player.Creature().CombatState().HittableEnemies();
        CreatureCmd::Damage(choiceContext, enemies, DynamicVars().Damage(), player.Creature());
    }
};

}  // namespace sts2
