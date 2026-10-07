// Models/Relics/BeautifulBracelet.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BeautifulBracelet : RelicModel {
    SIM_MODEL(BeautifulBracelet, RelicModel)

    void AfterObtained() const {
        CardList eligible;
        for (CardModel card : Owner().Deck().Cards())
            if (event_allowed::BaseCanEnchant(card)) eligible.push_back(card);
        Owner().RunState().Rng().Niche().Shuffle(eligible);
        int32_t count = std::min<int32_t>(4, static_cast<int32_t>(eligible.size()));
        for (int32_t i = 0; i < count; ++i)
            CardCmd::Enchant(EnchantmentId::SWIFT, eligible[static_cast<size_t>(i)], Dec(2));
    }
};

}  // namespace sts2
