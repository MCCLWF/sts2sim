// Models/Relics/VenerableTeaSet.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct VenerableTeaSet : RelicModel {
    SIM_MODEL(VenerableTeaSet, RelicModel)

    void AfterRoomEntered(AbstractRoom room) const {
        if (room.RoomType() != RoomType::RestSite) return;
        extra(0) = 1;
        SetStatus(RelicStatus::Active);
    }

    void AfterEnergyReset(Player player) const {
        if (player != Owner() || extra(0) == 0) return;
        PlayerCmd::GainEnergy(Dec(2), Owner());
        extra(0) = 0;
        SetStatus(RelicStatus::Normal);
    }
};

}  // namespace sts2
