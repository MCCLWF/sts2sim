// Models/Relics/Vajra.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
namespace sts2 {
struct Vajra : RelicModel {
    SIM_MODEL(Vajra, RelicModel)
    void AfterRoomEntered(AbstractRoom room) const {
        if (!room.IsCombatRoom()) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner().Creature(), DynamicVars().Strength().BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
