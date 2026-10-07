// Models/Powers/EscapeArtistPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct EscapeArtistPower : PowerModel {
    SIM_MODEL(EscapeArtistPower, PowerModel)
    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner()) && Amount() > 1) PowerCmd::Decrement(*this);
    }
};
}  // namespace sts2
