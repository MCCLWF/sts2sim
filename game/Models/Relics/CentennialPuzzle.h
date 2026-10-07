// Models/Relics/CentennialPuzzle.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct CentennialPuzzle : RelicModel {
    SIM_MODEL(CentennialPuzzle, RelicModel)

    void AfterDamageReceived(PlayerChoiceContext& ctx, Creature target, const DamageResult& result,
                             ValueProp, Creature, CardModel) const {
        if (!CombatManager(sim_).IsInProgress() || target != Owner().Creature() ||
            result.UnblockedDamage <= 0 || extra(0)) return;
        extra(0) = 1;
        for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i)
            CardPileCmd::Draw(ctx, Owner());
    }

    void AfterCombatEnd(CombatRoom) const { extra(0) = 0; }
};

}  // namespace sts2
