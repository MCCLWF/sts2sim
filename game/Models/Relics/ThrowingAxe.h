// Models/Relics/ThrowingAxe.cs (v0.111.0).
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct ThrowingAxe : RelicModel {
    SIM_MODEL(ThrowingAxe, RelicModel)

    bool UsedThisCombat() const { return extra(0) != 0; }

    void AfterRoomEntered(AbstractRoom room) const {
        if (!room.IsCombatRoom()) return;
        extra(0) = 0;
        SetStatus(RelicStatus::Active);
    }
    int32_t ModifyCardPlayCount(CardModel card, Creature, int32_t count) const {
        return !UsedThisCombat() && card.Owner() == Owner() ? count + 1 : count;
    }
    void AfterModifyingCardPlayCount(CardModel card) const {
        if (card.Owner() != Owner() || UsedThisCombat()) return;
        extra(0) = 1;
        Flash();
        SetStatus(RelicStatus::Normal);
    }
    void AfterCombatEnd(CombatRoom) const {
        extra(0) = 0;
        SetStatus(RelicStatus::Normal);
    }
};

}  // namespace sts2
