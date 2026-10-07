#pragma once

#include "game/Commands/ForgeCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {
struct FencingManual : RelicModel {
    SIM_MODEL(FencingManual, RelicModel)
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        Player owner = Owner();
        if (participants.contains(owner.Creature()) && owner.PlayerCombatState().TurnNumber() <= 1)
            ForgeCmd::Forge(DynamicVars().Forge().BaseValue(), owner, *this);
    }
};
}  // namespace sts2
