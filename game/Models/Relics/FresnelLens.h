// Models/Relics/FresnelLens.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct FresnelLens : RelicModel {
    SIM_MODEL(FresnelLens, RelicModel)

    static bool can_enchant(CardModel card) {
        return event_allowed::BaseCanEnchant(card) && card.GainsBlock();
    }

    CardModel EnchantCard(CardModel card) const {
        CardModel clone = Owner().RunState().CloneCard(card);
        CardCmd::Enchant(EnchantmentId::NIMBLE, clone, DynamicVars()["NimbleAmount"].BaseValue());
        return clone;
    }

    bool TryModifyCardRewardOptionsLate(Player player, CardList& cards, const CardCreationOptionsState&) const {
        if (player != Owner()) return false;
        for (CardModel& card : cards) if (can_enchant(card)) card = EnchantCard(card);
        return true;
    }

    void ModifyMerchantCardCreationResults(Player player, CardList& cards) const {
        if (player != Owner()) return;
        for (CardModel& card : cards) if (can_enchant(card)) card = EnchantCard(card);
    }

    bool TryModifyCardBeingAddedToDeck(CardModel card, CardModel& newCard) const {
        newCard = {};
        if (card.Owner() != Owner() || !can_enchant(card)) return false;
        newCard = EnchantCard(card);
        return true;
    }
};

}  // namespace sts2
