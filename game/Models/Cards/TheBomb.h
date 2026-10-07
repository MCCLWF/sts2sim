#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct TheBomb : CardModel {
    SIM_MODEL(TheBomb, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerModel power = PowerCmd::Apply(PowerId::THE_BOMB_POWER, ctx, Owner().Creature(), DynamicVars()["Turns"].BaseValue(),
                                           Owner().Creature(), *this);
        if (power) power.DynamicVars().Damage().SetBaseValue(DynamicVars()["BombDamage"].BaseValue());
    }

};

}  // namespace sts2
