#pragma once

#include "game/Models/RelicModel.h"
#include "game/Rewards/Reward.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct Driftwood : RelicModel {
    SIM_MODEL(Driftwood, RelicModel)

    bool TryModifyRewardsLate(Player player, RewardList rewards, AbstractRoom) const {
        if (player != Owner()) return false;
        for (size_t i = 0; i < rewards.size(); ++i) {
            Reward reward = rewards[i];
            if (reward.Type() == RewardType::Card) reward.st().can_reroll = true;
        }
        return true;
    }
};

}  // namespace sts2
