#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct MonarchsGaze : CardModel {
    SIM_MODEL(MonarchsGaze, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::MONARCHS_GAZE_POWER, ctx, Owner().Creature(), DynamicVars()["StrengthLoss"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
