#pragma once

// Models/Enchantments/Corrupted.cs (v0.111.0).
// Corrupted uses the base EnchantmentModel.CanEnchant implementation and only
// narrows the card type to Attack. Keep the complete predicate here so event
// selectors and CardCmd::Enchant share the same source-level rule.

#include "game/Models/CardModel.h"
#include "game/Models/EnchantmentModel.h"

namespace sts2 {

struct Corrupted : EnchantmentModel {
    SIM_MODEL(Corrupted, EnchantmentModel)

    static bool CanEnchantCardType(CardType cardType) { return cardType == CardType::Attack; }

    static bool CanEnchant(CardModel card) {
        if (!card) return false;
        const CardType type = card.Type();
        // EnchantmentModel.CanEnchant rejects Status, Curse, and Quest before
        // calling CanEnchantCardType.
        if (type == CardType::Status || type == CardType::Curse || type == CardType::Quest) return false;
        if (!CanEnchantCardType(type)) return false;
        CardPile pile = card.Pile();
        if (pile && pile.Type() == PileType::Deck && card.Keywords().Contains(CardKeyword::Unplayable)) return false;
        // Corrupted is not stackable, and a different existing enchantment is
        // rejected by the base predicate as well.
        if (card.Enchantment()) return false;
        return true;
    }

    static constexpr bool HasExtraCardText() { return true; }
};

}  // namespace sts2
