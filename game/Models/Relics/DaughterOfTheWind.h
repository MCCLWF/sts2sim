// Models/Relics/DaughterOfTheWind.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct DaughterOfTheWind : RelicModel {
    SIM_MODEL(DaughterOfTheWind, RelicModel)

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || play->Card.Type() != CardType::Attack) return;
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr, true);
    }
};

}  // namespace sts2
