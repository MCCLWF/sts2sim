// Models/Relics/ReptileTrinket.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct ReptileTrinket : RelicModel {
    SIM_MODEL(ReptileTrinket, RelicModel)

    void AfterPotionUsed(PotionModel potion, Creature) const {
        if (!potion || potion.Owner() != Owner() || !CombatManager(sim_).IsInProgress()) return;
        PlayerChoiceContext choiceContext = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::REPTILE_TRINKET_POWER, choiceContext, Owner().Creature(),
                        DynamicVars().Strength().BaseValue(), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
