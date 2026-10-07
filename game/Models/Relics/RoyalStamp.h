// Models/Relics/RoyalStamp.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Models/Events/AllowedHelpers.h"

namespace sts2 {

struct RoyalStamp : RelicModel {
    SIM_MODEL(RoyalStamp, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardList eligible;
        for (CardModel card : Owner().Deck().Cards())
            if (canApplyRoyalStamp(card)) eligible.push_back(card);
        // The source shuffles before opening the enchantment selector, whose
        // implementation then sorts the displayed choices back into deck order.
        CardList shuffled = eligible;
        Owner().RunState().Rng().Niche().Shuffle(shuffled);

        CardList selected = CardSelectCmd::Select(&sim(), eligible, 1, 1, "FromDeckForEnchantment");
        if (!selected.empty()) CardCmd::Enchant(EnchantmentId::ROYALLY_APPROVED, selected[0], Dec(1));
    }

private:
    static bool canApplyRoyalStamp(CardModel card) {
        if (!event_allowed::BaseCanEnchant(card)) return false;
        return card.Type() == CardType::Attack || card.Type() == CardType::Skill;
    }
};

}  // namespace sts2
