// Models/Relics/SealOfGold.cs.
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct SealOfGold : RelicModel {
    SIM_MODEL(SealOfGold, RelicModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        Player owner = Owner();
        if (!participants.contains(owner.Creature()) || owner.Gold() < DynamicVars().Gold().IntValue()) return;
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), owner);
        PlayerCmd::LoseGold(DynamicVars().Gold().IntValue(), owner);
    }
};

}  // namespace sts2
