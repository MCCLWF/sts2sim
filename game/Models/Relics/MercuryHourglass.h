// Models/Relics/MercuryHourglass.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct MercuryHourglass : RelicModel {
    SIM_MODEL(MercuryHourglass, RelicModel)
    void AfterPlayerTurnStart(PlayerChoiceContext& ctx, Player player) const {
        if (player == Owner()) {
            ExecutionSourceScope damageSource(player.Creature().sim(), ExecutionDamageSourceKind::Relic, data().entry);
            CreatureCmd::Damage(ctx, player.Creature().CombatState().HittableEnemies(), DynamicVars().Damage(),
                                Owner().Creature());
        }
    }
};

}  // namespace sts2
