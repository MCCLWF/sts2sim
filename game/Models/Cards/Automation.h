#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Automation : CardModel {
    SIM_MODEL(Automation, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::AUTOMATION_POWER, ctx, Owner().Creature(), DynamicVars().Energy().BaseValue(),
                        Owner().Creature(), *this);
    }

};

}  // namespace sts2
