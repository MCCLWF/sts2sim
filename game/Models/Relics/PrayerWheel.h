// Models/Relics/PrayerWheel.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
#include "game/Rewards/Reward.h"
#include "game/Runs/CardCreationOptions.h"

namespace sts2 {

struct PrayerWheel : RelicModel {
    SIM_MODEL(PrayerWheel, RelicModel)

    bool TryModifyRewards(Player player, RewardList rewards, AbstractRoom room) const {
        if (player != Owner() || !room || room.RoomType() != RoomType::Monster) return false;
        rewards.Add(Reward::NewCardReward(player, CardCreationOptions::ForRoom(player, RoomType::Monster), 3));
        return true;
    }
};

}  // namespace sts2
