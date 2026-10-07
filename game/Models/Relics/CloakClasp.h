// Models/Relics/CloakClasp.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct CloakClasp : RelicModel {
    SIM_MODEL(CloakClasp, RelicModel)

    void BeforeSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner().Creature())) return;
        int32_t handCount = Owner().PlayerCombatState().Hand().Count();
        if (handCount <= 0) return;
        Dec amount = DynamicVars()["Block"].BaseValue() * Dec(handCount);
        CreatureCmd::GainBlock(Owner().Creature(), amount, ValueProp::Unpowered, nullptr);
    }
};

}  // namespace sts2
