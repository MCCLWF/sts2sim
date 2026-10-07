// Models/Relics/Orrery.cs
#pragma once

#include "game/Commands/RewardsCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/CardCreationOptions.h"

namespace sts2 {

struct Orrery : RelicModel {
    SIM_MODEL(Orrery, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardCreationOptionsState options = CardCreationOptions::Make(
            {sts2::data(Owner().Character()).card_pool}, CardCreationSource::Other, CardRarityOddsType::RegularEncounter);
        StaticVec<Reward, 8> rewards;
        int32_t count = DynamicVars().Cards().IntValue();
        for (int32_t i = 0; i < count; ++i) rewards.push_back(Reward::NewCardReward(Owner(), options, 3));
        RewardsCmd::OfferCustom(Owner(), rewards);
    }
};

}  // namespace sts2
