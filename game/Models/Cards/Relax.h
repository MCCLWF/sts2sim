// Models/Cards/Relax.cs (v0.111.0); its upgrade deltas come from content tables.
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Relax : CardModel {
    SIM_MODEL(Relax, CardModel)

    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& context, const CardPlay* play) const {
        Creature owner = Owner().Creature();
        CreatureCmd::GainBlock(owner, DynamicVars().Block(), play);
        PowerCmd::Apply(PowerId::DRAW_CARDS_NEXT_TURN_POWER, context, owner,
                        DynamicVars().Cards().BaseValue(), owner, *this);
        PowerCmd::Apply(PowerId::ENERGY_NEXT_TURN_POWER, context, owner,
                        DynamicVars().Energy().BaseValue(), owner, *this);
    }
};

}  // namespace sts2
