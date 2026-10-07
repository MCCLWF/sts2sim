// Models/Relics/PunchDagger.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Models/Events/AllowedHelpers.h"

namespace sts2 {

struct PunchDagger : RelicModel {
    SIM_MODEL(PunchDagger, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardList selected = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), canApplyMomentum);
        int32_t amount = DynamicVars()["Momentum"].IntValue();
        for (CardModel card : selected) CardCmd::Enchant(EnchantmentId::MOMENTUM, card, Dec(amount));
    }

private:
    static bool canApplyMomentum(CardModel card) {
        return event_allowed::BaseCanEnchant(card) && card.Type() == CardType::Attack;
    }
};

}  // namespace sts2
