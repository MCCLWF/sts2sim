#pragma once

#include "game/Models/PowerModel.h"
#include "game/Rewards/Reward.h"

namespace sts2 {
struct RoyaltiesPower : PowerModel {
    SIM_MODEL(RoyaltiesPower, PowerModel)
    void AfterCombatEnd(CombatRoom) const {
        Reward reward = Reward::NewGoldReward(Owner().Player(), Amount());
        sim_->g->run.combat_extra_rewards.push_back(reward.Index());
    }
};
}  // namespace sts2
