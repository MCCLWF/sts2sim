// Models/Relics/EmberTea.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct EmberTea : RelicModel {
    SIM_MODEL(EmberTea, RelicModel)
    bool IsUsedUp() const { return extra(0) >= 5; }
    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return IsUsedUp() ? 0 : 5 - extra(0); }
    void AfterRoomEntered(AbstractRoom room) const {
        if (IsUsedUp() || !room.IsCombatRoom()) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner().Creature(), DynamicVars().Strength().BaseValue(),
                        Creature{}, CardModel{});
        ++extra(0);
        DynamicVars()["Combats"].SetBaseValue(Dec(DisplayAmount()));
        if (IsUsedUp()) SetStatus(RelicStatus::Disabled);
    }
};

}  // namespace sts2
