// Models/Cards/GatherLight.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct GatherLight : CardModel {
    SIM_MODEL(GatherLight, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
    }
};
}  // namespace sts2
