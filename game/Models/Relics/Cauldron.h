// Models/Relics/Cauldron.cs
#pragma once

#include "game/Commands/RewardsCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Cauldron : RelicModel {
    SIM_MODEL(Cauldron, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        StaticVec<Reward, 8> rewards;
        int32_t count = DynamicVars()["Potions"].IntValue();
        for (int32_t i = 0; i < count; ++i) rewards.push_back(Reward::NewPotionReward(Owner()));
        RewardsCmd::OfferCustom(Owner(), rewards);
    }
};

}  // namespace sts2
