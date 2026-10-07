// Models/Relics/MealTicket.cs (IsAllowed only: the relic grab bag asks every relic)
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct MealTicket : RelicModel {
    SIM_MODEL(MealTicket, RelicModel)
    void AfterRoomEntered(AbstractRoom room) const {
        if (!Owner().Creature().IsDead() && room.RoomType() == RoomType::Shop)
            CreatureCmd::Heal(Owner().Creature(), DynamicVars().Heal().BaseValue());
    }

    bool IsAllowed(RunState runState) const {
        return IsBeforeAct3TreasureChest(runState);
    }
};

}  // namespace sts2
