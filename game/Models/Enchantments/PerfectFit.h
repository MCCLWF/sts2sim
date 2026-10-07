// Models/Enchantments/PerfectFit.cs.
#pragma once

#include "game/Models/EnchantmentModel.h"

namespace sts2 {

struct PerfectFit : EnchantmentModel {
    SIM_MODEL(PerfectFit, EnchantmentModel)

    bool HasExtraCardText() const { return true; }

    void ModifyShuffleOrder(Player player, CardList& cards, bool isInitialShuffle) const {
        CardModel card = Card();
        if (isInitialShuffle || !card || card.Owner() != player || !cards.contains(card)) return;
        cards.remove(card);
        cards.insert(0, card);
    }
};

}  // namespace sts2
