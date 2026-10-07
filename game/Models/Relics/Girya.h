// Models/Relics/Girya.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Girya : RelicModel {
    SIM_MODEL(Girya, RelicModel)

    static constexpr int32_t kMaxLifts = 3;
    int32_t TimesLifted() const { return extra(0); }
    void IncrementTimesLifted() const { ++extra(0); }

    // TryModifyRestSiteOptions is implemented in RestSiteOption::Generate.

    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return TimesLifted(); }

    void AfterRoomEntered(AbstractRoom room) const {
        if (TimesLifted() <= 0 || !room.IsCombatRoom()) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner().Creature(), Dec(TimesLifted()), Owner().Creature(), CardModel{});
    }

    bool IsAllowed(RunState runState) const {
        return IsBeforeAct3TreasureChest(runState);
    }
};

}  // namespace sts2
