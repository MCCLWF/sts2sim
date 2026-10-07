#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct WraithForm : CardModel {
    SIM_MODEL(WraithForm, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::INTANGIBLE_POWER, ctx, Owner().Creature(), DynamicVars()["IntangiblePower"].BaseValue(),
                        Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::WRAITH_FORM_POWER, ctx, Owner().Creature(), DynamicVars()["WraithFormPower"].BaseValue(),
                        Owner().Creature(), *this);
    }
};

}  // namespace sts2
