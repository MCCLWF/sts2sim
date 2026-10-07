// Models/Events/FakeMerchant.cs (v0.111.0).
#pragma once

#include "engine/sim/alloc.h"
#include "game/Entities/Merchant/MerchantInventory.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Rewards/Reward.h"
#include "game/Runs/RunManager.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct FakeMerchant : EventModel {
    SIM_MODEL(FakeMerchant, EventModel)

    bool IsShared() const { return true; }
    EventOptionList GenerateInitialOptions() const { return {}; }

    void BeforeEventStarted(bool) const { Merchant::CreateForFakeMerchant(sim_, Rng()); }

    // OnEventFinished only appends unbought relics to map-point history.
    void OnEventFinished() const {}

    bool StartedFight() const { return extra(0) != 0; }

    void FoulPotionThrown() const {
        extra(0) = 1;
        StaticVec<RewardRef, 8> rewards;
        RelicModel rug(sim_, alloc_relic(*sim_->g, RelicId::FAKE_MERCHANTS_RUG, 0));
        rewards.push_back(Reward::NewRelicReward(Owner(), rug).Index());
        for (const MerchantRelicEntryState& entry : sim_->g->run.merchant.relics) {
            if (!entry.relic) continue;
            rewards.push_back(Reward::NewRelicReward(Owner(), RelicModel(sim_, entry.relic)).Index());
        }
        RunManager::EnterEventCombat(sim_, EncounterId::FAKE_MERCHANT_EVENT_ENCOUNTER,
                                     rewards, /*shouldResumeAfterCombat=*/false);
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const {
        if (runState.CurrentActIndex() < 1 || runState.Players().size() > 1) return false;
        return all_players(runState, [](Player player) {
            if (player.Gold() >= 100) return true;
            for (PotionModel potion : player.Potions())
                if (potion.PotionId_() == PotionId::FOUL_POTION) return true;
            return false;
        });
    }
};

}  // namespace sts2
