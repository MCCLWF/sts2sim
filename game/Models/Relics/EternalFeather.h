// Models/Relics/EternalFeather.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct EternalFeather : RelicModel {
    SIM_MODEL(EternalFeather, RelicModel)

    void AfterRoomEntered(AbstractRoom room) const {
        if (room.RoomType() != RoomType::RestSite) return;
        int32_t count = static_cast<int32_t>(Owner().Deck().Cards().size()) /
                        DynamicVars()["Cards"].IntValue();
        CreatureCmd::Heal(Owner().Creature(), DynamicVars()["Heal"].BaseValue() * Dec(count));
    }
};

}  // namespace sts2
