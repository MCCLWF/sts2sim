#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Anticipate : CardModel {
    SIM_MODEL(Anticipate, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ANTICIPATE_POWER, ctx, Owner().Creature(), DynamicVars().Dexterity().BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
