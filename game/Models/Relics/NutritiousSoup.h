// Models/Relics/NutritiousSoup.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct NutritiousSoup : RelicModel {
    SIM_MODEL(NutritiousSoup, RelicModel)

    void AfterObtained() const {
        for (CardModel card : Owner().Deck().Cards()) {
            if (card.Rarity() != CardRarity::Basic || !card.HasTag(CardTag::Strike) || card.Enchantment()) continue;
            CardCmd::Enchant(EnchantmentId::TEZCATARAS_EMBER, card, Dec(1));
        }
    }
};

}  // namespace sts2
