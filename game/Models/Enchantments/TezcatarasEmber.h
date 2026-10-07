// Models/Enchantments/TezcatarasEmber.cs (v0.111.0).
#pragma once

#include "game/Models/EnchantmentModel.h"
#include "game/Models/Events/AllowedHelpers.h"

namespace sts2 {

struct TezcatarasEmber : EnchantmentModel {
    SIM_MODEL(TezcatarasEmber, EnchantmentModel)

    static bool CanEnchant(CardModel card) { return event_allowed::BaseCanEnchant(card); }
};

}  // namespace sts2
