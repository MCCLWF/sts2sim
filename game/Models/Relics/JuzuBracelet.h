// Models/Relics/JuzuBracelet.cs (IsAllowed only: the relic grab bag asks every relic)
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct JuzuBracelet : RelicModel {
    SIM_MODEL(JuzuBracelet, RelicModel)

    bool IsAllowed(RunState runState) const {
        return IsBeforeAct3TreasureChest(runState);
    }

    RoomTypeSet ModifyUnknownMapPointRoomTypes(RoomTypeSet roomTypes) const {
        roomTypes.Remove(RoomType::Monster);
        return roomTypes;
    }
};

}  // namespace sts2
