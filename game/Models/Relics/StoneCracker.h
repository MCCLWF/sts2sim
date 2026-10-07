// Models/Relics/StoneCracker.cs
#pragma once

#include <algorithm>

#include "core/dotnet_sort.h"
#include "game/Commands/CardCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct StoneCracker : RelicModel {
    SIM_MODEL(StoneCracker, RelicModel)

    void AfterRoomEntered(AbstractRoom room) const {
        if (!room.IsCombatRoom()) return;
        CardList candidates = Owner().PlayerCombatState().DrawPile().Cards();
        candidates.remove_all([](CardModel card) { return !card.IsUpgradable(); });
        dotnet_sort(candidates.span(), [](CardModel a, CardModel b) { return a.CompareTo(b); });
        Owner().RunState().Rng().CombatCardSelection().Shuffle(candidates);
        int32_t count = std::min<int32_t>(DynamicVars().Cards().IntValue(), static_cast<int32_t>(candidates.size()));
        for (int32_t i = 0; i < count; ++i) CardCmd::Upgrade(candidates[static_cast<size_t>(i)]);
    }
};

}  // namespace sts2
