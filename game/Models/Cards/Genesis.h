#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Genesis : CardModel {
    SIM_MODEL(Genesis, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::GENESIS_POWER, ctx, Owner().Creature(), DynamicVars()["StarsPerTurn"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
