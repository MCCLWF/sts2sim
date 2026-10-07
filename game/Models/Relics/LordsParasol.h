// Models/Relics/LordsParasol.cs
#pragma once

#include "game/Entities/Merchant/MerchantInventory.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct LordsParasol : RelicModel {
    SIM_MODEL(LordsParasol, RelicModel)

    void AfterRoomEntered(AbstractRoom room) const {
        if (room.RoomType() != RoomType::Shop) return;
        for (Merchant::EntryRef entry : Merchant::AllEntries(&sim())) {
            if (entry.kind != Merchant::EntryRef::CardRemoval)
                Merchant::TryPurchase(&sim(), entry, /*ignoreCost=*/true);
        }
        for (Merchant::EntryRef entry : Merchant::AllEntries(&sim())) {
            if (entry.kind == Merchant::EntryRef::CardRemoval && Merchant::IsStocked(&sim(), entry))
                Merchant::TryPurchase(&sim(), entry, /*ignoreCost=*/true, /*cancelable=*/false);
        }
    }
};

}  // namespace sts2
