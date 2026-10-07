// Models/Relics/CharonsAshes.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct CharonsAshes : RelicModel {
    SIM_MODEL(CharonsAshes, RelicModel)
    void AfterCardExhausted(PlayerChoiceContext& ctx, CardModel card, bool) const {
        if (card.Owner() == Owner())
            CreatureCmd::Damage(ctx, Owner().Creature().CombatState().HittableEnemies(), DynamicVars().Damage().BaseValue(),
                                DynamicVars().Damage().Props(), Owner().Creature());
    }
};

}  // namespace sts2
