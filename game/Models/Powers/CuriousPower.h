#pragma once

#include "game/Models/PowerModel.h"

namespace sts2 {

struct CuriousPower : PowerModel {
    SIM_MODEL(CuriousPower, PowerModel)

    bool TryModifyEnergyCostInCombat(CardModel card, Dec original_cost, Dec& modified_cost) const {
        modified_cost = original_cost;
        if (!card || card.Owner().Creature() != Owner() || card.Type() != CardType::Power || original_cost <= Dec(0))
            return false;
        modified_cost = original_cost - Dec(Amount());
        if (modified_cost < Dec(0)) modified_cost = Dec(0);
        return true;
    }
};

}  // namespace sts2
