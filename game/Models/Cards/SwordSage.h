#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct SwordSage : CardModel {
    SIM_MODEL(SwordSage, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::SWORD_SAGE_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};
}  // namespace sts2
