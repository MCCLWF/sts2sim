// Models/Powers/HeistPower.cs
#pragma once

#include "game/Models/PowerModel.h"
#include "game/Rewards/Reward.h"

namespace sts2 {

struct HeistPower : PowerModel {
    SIM_MODEL(HeistPower, PowerModel)

    void BeforeDeath(Creature target) const {
        if (Owner() != target || !Target().Player()) return;
        Reward reward = Reward::NewGoldReward(Target().Player(), Amount(), true);
        sim_->g->run.combat_extra_rewards.push_back(reward.Index());
    }
};

}  // namespace sts2
