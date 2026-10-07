#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct FeedingFrenzy : CardModel {
    SIM_MODEL(FeedingFrenzy, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::FEEDING_FRENZY_POWER, ctx, Owner().Creature(), DynamicVars().Strength().BaseValue(),
                        Owner().Creature(), *this);
    }

};

}  // namespace sts2
