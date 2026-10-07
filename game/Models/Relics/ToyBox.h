// Models/Relics/ToyBox.cs (v0.111.0).
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct ToyBox : RelicModel {
    SIM_MODEL(ToyBox, RelicModel)

    bool IsUsedUp() const {
        return extra(0) >= DynamicVars()["Combats"].IntValue() * DynamicVars()["Relics"].IntValue();
    }
    bool HasUponPickupEffect() const { return true; }
    bool ShowCounter() const { return !IsUsedUp(); }
    int32_t DisplayAmount() const { return extra(0) % DynamicVars()["Combats"].IntValue(); }

    void AfterObtained() const {
        StaticVec<Reward, 8> rewards;
        for (int32_t i = 0; i < DynamicVars()["Relics"].IntValue(); ++i) {
            const RelicId id = RelicFactory::PullNextRelicFromFront(Owner());
            RelicModel wax(sim_, alloc_relic(*sim_->g, id, 0));
            wax.st().is_wax = true;
            rewards.push_back(Reward::NewRelicReward(Owner(), wax));
        }
        RewardsCmd::OfferCustom(Owner(), rewards);
    }

    void AfterCombatEnd(CombatRoom) const {
        if (IsUsedUp()) return;
        ++extra(0);
        if (extra(0) % DynamicVars()["Combats"].IntValue() != 0) return;
        for (RelicModel relic : Owner().Relics()) {
            if (relic.IsWax() && !relic.IsMelted()) {
                Owner().MeltRelicInternal(relic);
                break;
            }
        }
        if (IsUsedUp()) SetStatus(RelicStatus::Disabled);
    }
};

}  // namespace sts2
