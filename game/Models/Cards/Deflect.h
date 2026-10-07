// Models/Cards/Deflect.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Deflect : CardModel {
    SIM_MODEL(Deflect, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
    }
};
}
