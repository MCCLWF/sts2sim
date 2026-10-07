#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct IntimidatingHelmet : RelicModel {
    SIM_MODEL(IntimidatingHelmet, RelicModel)

    void BeforeCardPlayed(const CardPlay* cardPlay) const {
        if (!cardPlay || cardPlay->Card.Owner() != Owner() ||
            cardPlay->Resources.EnergyValue < DynamicVars().Energy().IntValue()) return;
        Flash();
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr);
    }
};

}  // namespace sts2
