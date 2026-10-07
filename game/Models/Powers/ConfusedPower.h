// Models/Powers/ConfusedPower.cs (v0.111.0).
#pragma once

#include "game/Models/PowerModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct ConfusedPower : PowerModel {
    SIM_MODEL(ConfusedPower, PowerModel)

    void AfterCardDrawn(PlayerChoiceContext&, CardModel card, bool) const {
        if (!card || card.Owner().Creature() != Owner() || card.EnergyCost().Canonical() < 0) return;
        int32_t cost = card.Owner().RunState().Rng().CombatEnergyCosts().NextInt(4);
        card.EnergyCost().SetThisCombat(cost);
    }
};

}  // namespace sts2
