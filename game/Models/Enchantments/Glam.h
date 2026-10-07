// Models/Enchantments/Glam.cs
#pragma once

#include "game/Models/EnchantmentModel.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Glam : EnchantmentModel {
    SIM_MODEL(Glam, EnchantmentModel)

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card != Card() || Card().st().enchantment_extra[0]) return;
        Card().st().enchantment_extra[0] = 1;
        Card().st().enchantment_status = EnchantmentStatus::Disabled;
    }
};

}  // namespace sts2
