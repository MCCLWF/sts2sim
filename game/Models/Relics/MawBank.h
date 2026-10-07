// Models/Relics/MawBank.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct MawBank : RelicModel {
    SIM_MODEL(MawBank, RelicModel)

    bool ShouldFlashOnPlayer() const { return false; }
    bool IsUsedUp() const { return extra(0) != 0; }

    void AfterRoomEntered(AbstractRoom) const {
        if (Owner().RunState().CurrentRoomCount() != 1 || IsUsedUp()) return;
        PlayerCmd::GainGold(DynamicVars().Gold().BaseValue(), Owner());
    }

    void AfterItemPurchased(Player player, Merchant::EntryRef, int32_t goldSpent) const {
        if (player != Owner() || IsUsedUp() || goldSpent <= 0) return;
        extra(0) = 1;
        SetStatus(RelicStatus::Disabled);
    }
};

}  // namespace sts2
