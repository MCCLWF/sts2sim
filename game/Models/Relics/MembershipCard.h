// Models/Relics/MembershipCard.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct MembershipCard : RelicModel {
    SIM_MODEL(MembershipCard, RelicModel)

    Dec ModifyMerchantPrice(Player player, Merchant::EntryRef, Dec originalPrice) const {
        if (player != Owner()) return originalPrice;
        return originalPrice * (DynamicVars()["Discount"].BaseValue() / Dec(100));
    }
};

}  // namespace sts2
