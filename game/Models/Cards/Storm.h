#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Storm : CardModel { SIM_MODEL(Storm, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::STORM_POWER, ctx, Owner().Creature(), DynamicVars()["StormPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
