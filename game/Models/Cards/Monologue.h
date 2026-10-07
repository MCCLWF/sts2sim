#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Monologue : CardModel {
    SIM_MODEL(Monologue, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerModel power = PowerCmd::Apply(PowerId::MONOLOGUE_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
        if (power) power.DynamicVars().Strength().SetBaseValue(DynamicVars()["Power"].BaseValue());
    }
};
}  // namespace sts2
