#pragma once

#include "game/Commands/RelicCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct SwordOfStone : RelicModel {
    SIM_MODEL(SwordOfStone, RelicModel)

    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return extra(0); }

    void AfterCombatVictory(CombatRoom room) const {
        if (room.RoomType() != RoomType::Elite) return;
        ++extra(0);
        if (extra(0) >= DynamicVars()["Elites"].BaseValue().to_int())
            RelicCmd::Replace(*this, RelicId::SWORD_OF_JADE);
    }
};

}  // namespace sts2
