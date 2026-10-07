// Models/Cards/CloakOfStars.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct CloakOfStars : CardModel {
    SIM_MODEL(CloakOfStars, CardModel)
    int32_t CanonicalStarCost() const { return 1; }
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
    }
};
}  // namespace sts2
