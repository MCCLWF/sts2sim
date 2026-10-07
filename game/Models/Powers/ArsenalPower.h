#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct ArsenalPower : PowerModel {
    SIM_MODEL(ArsenalPower, PowerModel)
    void AfterCardGeneratedForCombat(CardModel, Player creator) const {
        if (creator && creator.Creature() == Owner()) {
            PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
            PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(Amount()), Owner(), CardModel{});
        }
    }
};
}  // namespace sts2
