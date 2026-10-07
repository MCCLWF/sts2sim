// Models/Events/Nonupeipe.cs (v0.111.0).
#pragma once

#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct Nonupeipe : EventModel {
    SIM_MODEL(Nonupeipe, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList pool;
        pool.push_back(AncientRelicOption(RelicId::BLESSED_ANTLER));
        pool.push_back(AncientRelicOption(RelicId::BRILLIANT_SCARF));
        pool.push_back(AncientRelicOption(RelicId::DELICATE_FROND));
        pool.push_back(AncientRelicOption(RelicId::DIAMOND_DIADEM));
        pool.push_back(AncientRelicOption(RelicId::FUR_COAT));
        pool.push_back(AncientRelicOption(RelicId::GLITTER));
        pool.push_back(AncientRelicOption(RelicId::JEWELRY_BOX));
        pool.push_back(AncientRelicOption(RelicId::LOOMING_FRUIT));
        pool.push_back(AncientRelicOption(RelicId::SIGNET_RING));
        int32_t eligible = 0;
        for (CardModel card : Owner().Deck().Cards())
            if (event_allowed::BaseCanEnchant(card)) ++eligible;
        if (eligible >= 4) pool.push_back(AncientRelicOption(RelicId::BEAUTIFUL_BRACELET));
        Rng().Shuffle(pool);
        EventOptionList result;
        for (size_t i = 0; i < pool.size() && i < 3; ++i) result.push_back(pool[i]);
        return result;
    }
};

}  // namespace sts2
