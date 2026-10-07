#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Hailstorm : CardModel { SIM_MODEL(Hailstorm, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::HAILSTORM_POWER, ctx, Owner().Creature(), DynamicVars()["HailstormPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
