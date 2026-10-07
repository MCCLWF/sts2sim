// Models/Enchantments/Goopy.cs (v0.111.0).
#pragma once

#include "game/Entities/Cards/CardPlay.h"
#include "game/Models/EnchantmentModel.h"
#include "game/Models/Events/AllowedHelpers.h"

namespace sts2 {

struct Goopy : EnchantmentModel {
    SIM_MODEL(Goopy, EnchantmentModel)

    static bool CanEnchant(CardModel card) {
        return event_allowed::BaseCanEnchant(card) && card.HasTag(CardTag::Defend);
    }
    static void OnEnchant(CardModel card) { card.AddKeyword(CardKeyword::Exhaust); }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card != Card()) return;
        ++Card().st().enchantment_amount;
        CardModel deckVersion = Card().DeckVersion();
        if (deckVersion && deckVersion.Enchantment()) ++deckVersion.st().enchantment_amount;
    }
};

}  // namespace sts2
