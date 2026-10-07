// Models/Relics/BlackStar.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
#include "game/Rewards/Reward.h"

namespace sts2 {

struct BlackStar : RelicModel {
    SIM_MODEL(BlackStar, RelicModel)

    bool TryModifyRewards(Player player, RewardList rewards, AbstractRoom room) const {
        if (player != Owner() || !room || room.RoomType() != RoomType::Elite) return false;
        rewards.Add(Reward::NewRelicReward(player));
        return true;
    }
};

}  // namespace sts2
