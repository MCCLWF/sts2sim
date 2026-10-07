// Models/Relics/TheCourier.cs (IsAllowedInShops only: the merchant asks every relic)
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct TheCourier : RelicModel {
    SIM_MODEL(TheCourier, RelicModel)

    bool IsAllowedInShops() const { return false; }
    bool ShouldRefillMerchantEntry(Merchant::EntryRef, Player player) const { return player == Owner(); }
    Dec ModifyMerchantPrice(Player player, Merchant::EntryRef, Dec originalPrice) const {
        if (player != Owner()) return originalPrice;
        return originalPrice * (Dec(1) - DynamicVars()["Discount"].BaseValue() / Dec(100));
    }
};

}  // namespace sts2
