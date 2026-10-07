// Models/Relics/ForgottenSoul.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct ForgottenSoul : RelicModel {
    SIM_MODEL(ForgottenSoul, RelicModel)

    void AfterCardExhausted(PlayerChoiceContext& choiceContext, CardModel card, bool) const {
        if (card.Owner() != Owner()) return;
        Flash();
        CreatureList enemies = Owner().Creature().CombatState().HittableEnemies();
        Creature target = Owner().RunState().Rng().CombatTargets().NextItem(enemies);
        if (target) CreatureCmd::Damage(choiceContext, target, DynamicVars().Damage(), Owner().Creature());
    }
};

}  // namespace sts2
