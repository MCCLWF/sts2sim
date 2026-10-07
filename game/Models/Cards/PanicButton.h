#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct PanicButton : CardModel {
    SIM_MODEL(PanicButton, CardModel)
    bool GainsBlock() const { return true; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        PowerCmd::Apply(PowerId::NO_BLOCK_POWER, ctx, Owner().Creature(), DynamicVars()["Turns"].BaseValue(),
                        Owner().Creature(), *this);
    }
};

}  // namespace sts2
