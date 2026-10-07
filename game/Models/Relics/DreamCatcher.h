// Models/Relics/DreamCatcher.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Rewards/Reward.h"
#include "game/Runs/CardCreationOptions.h"

namespace sts2 {

struct DreamCatcher : RelicModel {
    SIM_MODEL(DreamCatcher, RelicModel)

    bool TryModifyRestSiteHealRewards(Player player, RewardList rewards, bool) const {
        if (player != Owner()) return false;
        rewards.Add(Reward::NewCardReward(player, CardCreationOptions::ForRoom(player, RoomType::Monster), 3));
        return true;
    }
};

}  // namespace sts2
