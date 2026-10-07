#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Accuracy : CardModel {
    SIM_MODEL(Accuracy, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ACCURACY_POWER, ctx, Owner().Creature(), DynamicVars()["AccuracyPower"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
