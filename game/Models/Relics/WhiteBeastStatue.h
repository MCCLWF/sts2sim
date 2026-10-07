// Models/Relics/WhiteBeastStatue.cs (IsAllowed only: the relic grab bag asks every relic)
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct WhiteBeastStatue : RelicModel {
    SIM_MODEL(WhiteBeastStatue, RelicModel)
    bool ShouldForcePotionReward(Player player, RoomType room) const {
        return player == Owner() && (room == RoomType::Monster || room == RoomType::Elite || room == RoomType::Boss);
    }

    bool IsAllowed(RunState runState) const {
        return IsBeforeAct3TreasureChest(runState);
    }
};

}  // namespace sts2
