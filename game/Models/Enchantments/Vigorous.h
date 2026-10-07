// Models/Enchantments/Vigorous.cs (v0.111.0).
#pragma once

#include "game/Models/CardModel.h"
#include "game/Models/EnchantmentModel.h"
#include "game/Models/Events/AllowedHelpers.h"

namespace sts2 {

struct Vigorous : EnchantmentModel {
    SIM_MODEL(Vigorous, EnchantmentModel)

    static bool CanEnchant(CardModel card) {
        return event_allowed::BaseCanEnchant(card) && card.Type() == CardType::Attack;
    }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (play && play->Card == Card())
            Card().st().enchantment_status = EnchantmentStatus::Disabled;
    }
};

}  // namespace sts2
