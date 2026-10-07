// Models/Relics/Kifuda.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Kifuda : RelicModel {
    SIM_MODEL(Kifuda, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const {
        CardSelectorPrefs prefs(0, DynamicVars().Cards().IntValue());
        prefs.Cancelable = false;
        prefs.RequireManualConfirmation = true;
        CardList cards = CardSelectCmd::FromDeckForEnchantment(Owner(), prefs, event_allowed::BaseCanEnchant);
        for (CardModel card : cards) CardCmd::Enchant(EnchantmentId::ADROIT, card, Dec(3));
    }
};

}  // namespace sts2
