// Models/Relics/TriBoomerang.cs.
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct TriBoomerang : RelicModel {
    SIM_MODEL(TriBoomerang, RelicModel)

    void AfterObtained() const {
        CardSelectorPrefs prefs(DynamicVars().Cards().IntValue());
        CardList selected = CardSelectCmd::FromDeckForEnchantment(Owner(), prefs, canEnchant);
        for (CardModel card : selected) CardCmd::Enchant(EnchantmentId::INSTINCT, card, Dec(1));
    }

private:
    static bool canEnchant(CardModel card) {
        if (!card || card.Type() != CardType::Attack || card.Enchantment()) return false;
        if (card.Keywords().Contains(CardKeyword::Unplayable)) return false;
        return true;
    }
};

}  // namespace sts2
