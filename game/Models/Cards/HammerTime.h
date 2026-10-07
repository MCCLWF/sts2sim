// Models/Cards/HammerTime.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct HammerTime : CardModel {
    SIM_MODEL(HammerTime, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::HAMMER_TIME_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};
}  // namespace sts2
