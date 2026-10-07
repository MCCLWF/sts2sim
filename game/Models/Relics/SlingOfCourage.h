#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct SlingOfCourage : RelicModel {
    SIM_MODEL(SlingOfCourage, RelicModel)

    void AfterRoomEntered(AbstractRoom room) const {
        if (room.RoomType() != RoomType::Elite) return;
        Flash();
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner().Creature(),
                        DynamicVars().Strength().BaseValue(), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
