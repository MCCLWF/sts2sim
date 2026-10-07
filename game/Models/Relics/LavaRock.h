// Models/Relics/LavaRock.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Rewards/Reward.h"
namespace sts2 {
struct LavaRock : RelicModel {
    SIM_MODEL(LavaRock, RelicModel)
    bool ShowCounter() const { return false; }
    bool TryModifyRewards(Player player, RewardList rewards, AbstractRoom room) const {
        if (player != Owner() || !room || room.RoomType() != RoomType::Boss || Owner().RunState().CurrentActIndex() != 0 || extra(0)) return false;
        for (int i = 0; i < 2; ++i) rewards.Add(Reward::NewRelicReward(player));
        extra(0) = 1;
        SetStatus(RelicStatus::Disabled);
        return true;
    }
};
}
