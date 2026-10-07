// Models/Enchantments/SoulsPower.cs (v0.111.0).
#pragma once

#include "game/Entities/Cards/CardPile.h"
#include "game/Models/CardModel.h"
#include "game/Models/EnchantmentModel.h"

namespace sts2 {

struct SoulsPower : EnchantmentModel {
    SIM_MODEL(SoulsPower, EnchantmentModel)

    static bool CanEnchant(CardModel card) {
        if (!card) return false;
        const CardType type = card.Type();
        if (type == CardType::Status || type == CardType::Curse || type == CardType::Quest) return false;
        CardPile pile = card.Pile();
        if (pile && pile.Type() == PileType::Deck && card.Keywords().Contains(CardKeyword::Unplayable)) return false;
        if (card.Enchantment()) return false;  // SoulsPower is not stackable.
        return card.GetKeywordsWithSources(KeywordSources::Local).Contains(CardKeyword::Exhaust);
    }

    static void OnEnchant(CardModel card) { card.RemoveKeyword(CardKeyword::Exhaust); }
};

}  // namespace sts2
