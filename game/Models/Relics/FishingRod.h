// Models/Relics/FishingRod.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardCmd.h"
#include "game/Rooms/AbstractRoom.h"
namespace sts2 {
struct FishingRod : RelicModel {
    SIM_MODEL(FishingRod, RelicModel)
    int32_t DisplayAmount() const { return extra(0) % 3; }
    bool ShowCounter() const { return true; }
    void AfterCombatEnd(CombatRoom room) const {
        if (room.RoomType() != RoomType::Monster) return;
        ++extra(0);
        if (extra(0) % 3 != 0) return;
        CardList upgradable;
        for (CardModel c : Owner().Deck().Cards()) if (c.IsUpgradable()) upgradable.push_back(c);
        if (!upgradable.empty()) {
            int32_t i = Owner().RunState().Rng().Niche().next_item_index(static_cast<int32_t>(upgradable.size()));
            CardCmd::Upgrade(upgradable[static_cast<size_t>(i)]);
        }
    }
};
}
