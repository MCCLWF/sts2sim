#pragma once

#include <algorithm>

#include "game/Models/RelicModel.h"
#include "game/Rewards/Reward.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct WongosMysteryTicket : RelicModel {
    SIM_MODEL(WongosMysteryTicket, RelicModel)

    static constexpr int32_t combatsToActivate = 5;
    static constexpr int32_t relicCount = 3;

    // SavedProperty state: CombatsFinished and GaveRelic.
    int32_t CombatsFinished() const { return extra(0); }
    bool GaveRelic() const { return extra(1) != 0; }

    bool IsUsedUp() const { return GaveRelic(); }
    bool ShowCounter() const { return DisplayAmount() > 0; }
    int32_t DisplayAmount() const { return combatsToActivate - CombatsFinished(); }

    void AfterCombatEnd(CombatRoom) const {
        ++extra(0);
        int32_t remaining = combatsToActivate - CombatsFinished();
        DynamicVars()["RemainingCombats"].SetBaseValue(Dec(std::max(remaining, 0)));
    }

    bool TryModifyRewards(Player player, RewardList rewards, AbstractRoom room) const {
        if (player != Owner() || !room || !room.IsCombatRoom() || GaveRelic()) return false;
        if (combatsToActivate - CombatsFinished() > 0) return false;

        for (int32_t i = 0; i < DynamicVars().Repeat().IntValue(); ++i) {
            rewards.Add(Reward::NewRelicReward(player));
        }
        return true;
    }

    void AfterModifyingRewards() const {
        extra(1) = 1;
        SetStatus(RelicStatus::Disabled);
    }
};

}  // namespace sts2
