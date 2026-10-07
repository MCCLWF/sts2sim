#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Abrasive : CardModel {
    SIM_MODEL(Abrasive, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, Owner().Creature(), DynamicVars().Dexterity().BaseValue(), Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::THORNS_POWER, ctx, Owner().Creature(), DynamicVars()["ThornsPower"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
