// Models/Relics/SelfFormingClay.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct SelfFormingClay : RelicModel {
    SIM_MODEL(SelfFormingClay, RelicModel)

    void AfterDamageReceived(PlayerChoiceContext& choiceContext, Creature target,
                             const DamageResult& result, ValueProp, Creature, CardModel) const {
        if (!CombatManager(sim_).IsInProgress() || target != Owner().Creature() ||
            result.UnblockedDamage <= 0) return;
        PowerCmd::Apply(PowerId::SELF_FORMING_CLAY_POWER, choiceContext, target,
                        DynamicVars()["BlockNextTurn"].BaseValue(), target, CardModel{});
    }
};

}  // namespace sts2
