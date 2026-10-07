// Models/Events/Tanx.cs (v0.111.0): ancient relic option generation.
#pragma once

#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct Tanx : EventModel {
    SIM_MODEL(Tanx, EventModel)

    EventOptionList GenerateInitialOptions() const {
        // BaseOptionPool.ToList materializes every mutable relic option before
        // the conditional TriBoomerang option and the unstable shuffle.
        EventOptionList pool;
        pool.push_back(AncientRelicOption(RelicId::CLAWS));
        pool.push_back(AncientRelicOption(RelicId::CROSSBOW));
        pool.push_back(AncientRelicOption(RelicId::IRON_CLUB));
        pool.push_back(AncientRelicOption(RelicId::MEAT_CLEAVER));
        pool.push_back(AncientRelicOption(RelicId::SAI));
        pool.push_back(AncientRelicOption(RelicId::SPIKED_GAUNTLETS));
        pool.push_back(AncientRelicOption(RelicId::TANXS_WHISTLE));
        pool.push_back(AncientRelicOption(RelicId::THROWING_AXE));
        pool.push_back(AncientRelicOption(RelicId::WAR_HAMMER));

        int eligible = 0;
        for (CardModel card : Owner().Deck().Cards()) {
            if (event_allowed::BaseCanEnchant(card) && card.Type() == CardType::Attack) ++eligible;
        }
        if (eligible >= 3) pool.push_back(AncientRelicOption(RelicId::TRI_BOOMERANG));

        Rng().Shuffle(pool);
        EventOptionList options;
        for (size_t i = 0; i < 3; ++i) options.push_back(pool[i]);
        return options;
    }
};

}  // namespace sts2
