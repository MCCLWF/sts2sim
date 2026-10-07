// Models/Relics/LostWisp.cs (v0.111.0).
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct LostWispRelic : RelicModel {
    SIM_MODEL(LostWispRelic, RelicModel)

    void AfterCardPlayed(PlayerChoiceContext& context, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !CombatManager(sim_).IsInProgress() ||
            play->Card.Type() != CardType::Power) return;
        CreatureCmd::Damage(context, Owner().Creature().CombatState().HittableEnemies(),
                            DynamicVars().Damage(), Owner().Creature());
    }
};

}  // namespace sts2
