// Models/Relics/DivineRight.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {
struct DivineRight : RelicModel {
    SIM_MODEL(DivineRight, RelicModel)
    void AfterRoomEntered(AbstractRoom room) const {
        if (room.IsCombatRoom()) PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
    }
};
}  // namespace sts2
