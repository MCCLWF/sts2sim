#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct DuplicationPower : PowerModel {
    SIM_MODEL(DuplicationPower, PowerModel)

    int32_t ModifyCardPlayCount(CardModel card, Creature, int32_t count) const {
        return card.Owner().Creature() == Owner() ? count + 1 : count;
    }

    void AfterModifyingCardPlayCount(CardModel card) const {
        if (card.Owner().Creature() == Owner()) PowerCmd::Decrement(*this);
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
