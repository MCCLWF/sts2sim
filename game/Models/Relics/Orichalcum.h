#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Orichalcum : RelicModel {
    SIM_MODEL(Orichalcum, RelicModel)
    void BeforeSideTurnEndVeryEarly(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner().Creature()) && Owner().Creature().Block() <= Dec(0)) extra(0) = 1;
    }
    void BeforeSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList&) const {
        if (extra(0)) { extra(0) = 0; CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block().BaseValue(), ValueProp::Unpowered, nullptr); }
    }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature())) extra(0) = 0;
    }
};

}  // namespace sts2
