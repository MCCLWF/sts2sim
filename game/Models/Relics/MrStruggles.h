// Models/Relics/MrStruggles.cs.
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct MrStruggles : RelicModel {
    SIM_MODEL(MrStruggles, RelicModel)

    void AfterPlayerTurnStart(PlayerChoiceContext& context, Player player) const {
        if (player != Owner()) return;
        CombatState combat = player.Creature().CombatState();
        CreatureCmd::Damage(context, combat.HittableEnemies(), Dec(player.PlayerCombatState().TurnNumber()),
                            ValueProp::Unpowered, Owner().Creature());
    }
};

}  // namespace sts2
