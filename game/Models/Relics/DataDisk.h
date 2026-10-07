// Models/Relics/DataDisk.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
namespace sts2 {
struct DataDisk : RelicModel { SIM_MODEL(DataDisk, RelicModel)
    void AfterRoomEntered(AbstractRoom room) const {
        if (!room.IsCombatRoom()) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FOCUS_POWER, ctx, Owner().Creature(),
                        DynamicVars()["FocusPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
