#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct SerpentForm : CardModel {
    SIM_MODEL(SerpentForm, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::SERPENT_FORM_POWER, ctx, Owner().Creature(),
                        DynamicVars()["SerpentFormPower"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
