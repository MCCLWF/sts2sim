#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {
struct Regalite : RelicModel {
    SIM_MODEL(Regalite, RelicModel)
    void AfterCardGeneratedForCombat(CardModel, Player creator) const {
        if (!creator || creator != Owner() || extra(0)) return;
        extra(0) = 1;
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr, true);
    }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature())) extra(0) = 0;
    }
    void AfterCombatEnd(CombatRoom) const { extra(0) = 0; }
};
}  // namespace sts2
