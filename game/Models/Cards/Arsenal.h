#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Arsenal : CardModel {
    SIM_MODEL(Arsenal, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ARSENAL_POWER, ctx, Owner().Creature(), DynamicVars()["ArsenalPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
