#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct ChildOfTheStars : CardModel {
    SIM_MODEL(ChildOfTheStars, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::CHILD_OF_THE_STARS_POWER, ctx, Owner().Creature(), DynamicVars()["BlockForStars"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
