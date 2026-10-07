// Models/Cards/DefendIronclad.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct DefendIronclad : CardModel {
    SIM_MODEL(DefendIronclad, CardModel)

    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), cardPlay);
    }
};

}  // namespace sts2
