// Models/Relics/CallingBell.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct CallingBell : RelicModel {
    SIM_MODEL(CallingBell, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardPileCmd::AddCurseToDeck(CardId::CURSE_OF_THE_BELL, Owner());
        StaticVec<Reward, 8> rewards;
        rewards.push_back(Reward::NewRelicReward(Owner(), RelicRarity::Common));
        rewards.push_back(Reward::NewRelicReward(Owner(), RelicRarity::Uncommon));
        rewards.push_back(Reward::NewRelicReward(Owner(), RelicRarity::Rare));
        RewardsCmd::OfferCustom(Owner(), rewards);
    }
};

}  // namespace sts2
