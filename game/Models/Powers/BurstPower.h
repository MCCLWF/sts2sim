#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct BurstPower : PowerModel {
    SIM_MODEL(BurstPower, PowerModel)

    int32_t ModifyCardPlayCount(CardModel card, Creature, int32_t playCount) const {
        return card && card.Owner().Creature() == Owner() && card.Type() == CardType::Skill ? playCount + 1 : playCount;
    }

    void AfterModifyingCardPlayCount(CardModel card) const {
        if (card && card.Owner().Creature() == Owner() && card.Type() == CardType::Skill) PowerCmd::Decrement(*this);
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
