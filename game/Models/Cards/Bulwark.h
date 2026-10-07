#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/ForgeCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Bulwark : CardModel {
    SIM_MODEL(Bulwark, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        ForgeCmd::Forge(DynamicVars().Forge().IntValue(), Owner(), *this);
    }
};
}  // namespace sts2
