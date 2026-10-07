#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct ToughBandages : RelicModel {
    SIM_MODEL(ToughBandages, RelicModel)

    void AfterCardDiscarded(PlayerChoiceContext&, CardModel card) const {
        if (card.Owner() != Owner() || Owner().Creature().Side() != Owner().Creature().CombatState().CurrentSide()) return;
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr);
    }
};

}  // namespace sts2
