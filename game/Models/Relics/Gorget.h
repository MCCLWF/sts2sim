// Models/Relics/Gorget.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct Gorget : RelicModel {
    SIM_MODEL(Gorget, RelicModel)

    void AfterRoomEntered(AbstractRoom room) const {
        if (!room.IsCombatRoom()) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::PLATING_POWER, ctx, Owner().Creature(), DynamicVars()["PlatingPower"].BaseValue(),
                        Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
