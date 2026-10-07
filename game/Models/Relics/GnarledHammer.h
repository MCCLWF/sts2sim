// Models/Relics/GnarledHammer.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Models/Events/AllowedHelpers.h"

namespace sts2 {

struct GnarledHammer : RelicModel {
    SIM_MODEL(GnarledHammer, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardSelectorPrefs prefs(0, DynamicVars().Cards().IntValue());
        CardList selected = CardSelectCmd::FromDeckForEnchantment(Owner(), prefs, canApplySharp);
        int32_t amount = DynamicVars()["SharpAmount"].IntValue();
        for (CardModel card : selected) CardCmd::Enchant(EnchantmentId::SHARP, card, Dec(amount));
    }

private:
    static bool canApplySharp(CardModel card) {
        return event_allowed::BaseCanEnchant(card) && card.Type() == CardType::Attack;
    }
};

}  // namespace sts2
