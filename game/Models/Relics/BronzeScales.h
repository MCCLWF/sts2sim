// Models/Relics/BronzeScales.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct BronzeScales : RelicModel {
    SIM_MODEL(BronzeScales, RelicModel)

    void AfterRoomEntered(AbstractRoom room) const {
        if (!room.IsCombatRoom()) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::THORNS_POWER, ctx, Owner().Creature(), Dec(3), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
