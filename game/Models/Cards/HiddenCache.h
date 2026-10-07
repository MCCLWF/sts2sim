#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct HiddenCache : CardModel {
    SIM_MODEL(HiddenCache, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
        PowerCmd::Apply(PowerId::STAR_NEXT_TURN_POWER, ctx, Owner().Creature(), DynamicVars()["StarNextTurnPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
