#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct Enlightenment : CardModel {
    SIM_MODEL(Enlightenment, CardModel)

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        for (CardModel card : Owner().PlayerCombatState().Hand().Cards()) {
            if (CurrentUpgradeLevel() > 0) card.EnergyCost().SetThisCombat(1, true);
            else card.EnergyCost().SetThisTurnOrUntilPlayed(1, true);
        }
    }
};

}  // namespace sts2
