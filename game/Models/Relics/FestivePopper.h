// Models/Relics/FestivePopper.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct FestivePopper : RelicModel {
    SIM_MODEL(FestivePopper, RelicModel)
    void AfterPlayerTurnStart(PlayerChoiceContext& ctx, Player player) const {
        if (player == Owner() && Owner().PlayerCombatState().TurnNumber() == 1)
            CreatureCmd::Damage(ctx, Owner().Creature().CombatState().HittableEnemies(), DynamicVars().Damage(),
                                Owner().Creature());
    }
};

}  // namespace sts2
