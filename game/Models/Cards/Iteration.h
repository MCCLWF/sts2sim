#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Iteration : CardModel { SIM_MODEL(Iteration, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ITERATION_POWER, ctx, Owner().Creature(), DynamicVars()["IterationPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
