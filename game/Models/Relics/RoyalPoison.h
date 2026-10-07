// Models/Relics/RoyalPoison.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct RoyalPoison : RelicModel {
    SIM_MODEL(RoyalPoison, RelicModel)

    void AfterPlayerTurnStart(PlayerChoiceContext& context, Player player) const {
        if (player == Owner() && Owner().PlayerCombatState().TurnNumber() <= 1)
            CreatureCmd::Damage(context, Owner().Creature(), DynamicVars().Damage(), Creature{});
    }
};

}  // namespace sts2
