// Models/Relics/OddlySmoothStone.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
namespace sts2 {
struct OddlySmoothStone : RelicModel {
    SIM_MODEL(OddlySmoothStone, RelicModel)
    void AfterRoomEntered(AbstractRoom room) const {
        if (!room.IsCombatRoom()) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, Owner().Creature(), DynamicVars().Dexterity().BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
