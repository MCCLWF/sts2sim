#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct SwordOfJade : RelicModel {
    SIM_MODEL(SwordOfJade, RelicModel)

    void AfterRoomEntered(AbstractRoom room) const {
        if (!room.IsCombatRoom()) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner().Creature(), DynamicVars().Strength().BaseValue(),
                        Creature{}, CardModel{});  // applier: null
    }
};

}  // namespace sts2
