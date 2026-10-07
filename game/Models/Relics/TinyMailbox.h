// Models/Relics/TinyMailbox.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Rewards/Reward.h"

namespace sts2 {

struct TinyMailbox : RelicModel {
    SIM_MODEL(TinyMailbox, RelicModel)

    bool TryModifyRestSiteHealRewards(Player player, RewardList rewards, bool) const {
        if (player != Owner()) return false;
        rewards.Add(Reward::NewPotionReward(player));
        rewards.Add(Reward::NewPotionReward(player));
        Flash();
        return true;
    }
};

}  // namespace sts2
