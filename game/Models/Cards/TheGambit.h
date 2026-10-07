#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct TheGambit : CardModel {
    SIM_MODEL(TheGambit, CardModel)

    bool GainsBlock() const { return true; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        PowerCmd::Apply(PowerId::THE_GAMBIT_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }

};

}  // namespace sts2
