// Models/Relics/WhiteStar.cs (IsAllowed only: the relic grab bag asks every relic)
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"
#include "game/Rewards/Reward.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct WhiteStar : RelicModel {
    SIM_MODEL(WhiteStar, RelicModel)
    bool TryModifyRewards(Player player, RewardList rewards, AbstractRoom room) const {
        if (player != Owner() || !room || room.RoomType() != RoomType::Elite) return false;
        rewards.Add(Reward::NewCardReward(player, CardCreationOptions::ForRoom(player, RoomType::Boss), 3));
        return true;
    }

    bool IsAllowed(RunState runState) const {
        return IsBeforeAct3TreasureChest(runState);
    }
};

}  // namespace sts2
