#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct TheSealedThrone : CardModel {
    SIM_MODEL(TheSealedThrone, CardModel)
    int32_t CanonicalStarCost() const { return 3; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::THE_SEALED_THRONE_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};
}  // namespace sts2
