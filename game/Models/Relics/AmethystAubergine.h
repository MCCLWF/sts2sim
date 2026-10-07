// Models/Relics/AmethystAubergine.cs (IsAllowed and IsAllowedInShops: the relic grab bag and the merchant ask every relic)
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"
#include "game/Rewards/Reward.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct AmethystAubergine : RelicModel {
    SIM_MODEL(AmethystAubergine, RelicModel)
    bool TryModifyRewards(Player player, RewardList rewards, AbstractRoom room) const {
        if (player != Owner() || !room || !room.IsCombatRoom()) return false;
        if (room.RoomType() == RoomType::Boss && player.RunState().CurrentActIndex() >=
            static_cast<int32_t>(player.RunState().st().acts.size()) - 1) return false;
        rewards.Add(Reward::NewGoldReward(player, DynamicVars().Gold().IntValue()));
        return true;
    }
    void AfterModifyingRewards() const {}

    bool IsAllowedInShops() const { return false; }

    bool IsAllowed(RunState runState) const {
        return IsBeforeAct3TreasureChest(runState);
    }
};

}  // namespace sts2
