// Models/Relics/WingCharm.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct WingCharm : RelicModel {
    SIM_MODEL(WingCharm, RelicModel)

    bool TryModifyCardRewardOptionsLate(Player player, CardList& cardRewards,
                                        const CardCreationOptionsState&) const {
        if (player != Owner()) return false;
        CardList eligible;
        for (CardModel card : cardRewards)
            if (event_allowed::BaseCanEnchant(card)) eligible.push_back(card);
        if (eligible.empty()) return false;
        CardModel selected = Owner().RunState().Rng().Niche().NextItem(eligible);
        CardModel enchanted = Owner().RunState().CloneCard(selected);
        CardCmd::Enchant(EnchantmentId::SWIFT, enchanted, DynamicVars()["SwiftAmount"].BaseValue());
        for (CardModel& card : cardRewards) {
            if (card == selected) {
                card = enchanted;
                break;
            }
        }
        return true;
    }
};

}  // namespace sts2
