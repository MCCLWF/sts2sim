#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Mirage : CardModel {
    SIM_MODEL(Mirage, CardModel)

    bool GainsBlock() const { return true; }

    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        Creature target = play ? play->Target : Creature{};
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().CalculatedBlock().Calculate(target),
                               DynamicVars().CalculatedBlock().Props(), play);
    }
};

}  // namespace sts2
