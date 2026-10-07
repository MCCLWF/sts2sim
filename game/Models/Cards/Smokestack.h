#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Smokestack : CardModel { SIM_MODEL(Smokestack, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::SMOKESTACK_POWER, ctx, Owner().Creature(), DynamicVars()["SmokestackPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
