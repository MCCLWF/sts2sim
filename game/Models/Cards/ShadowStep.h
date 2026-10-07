#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct ShadowStep : CardModel {
    SIM_MODEL(ShadowStep, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardCmd::Discard(ctx, Owner().PlayerCombatState().Hand().Cards());
        PowerCmd::Apply(PowerId::SHADOW_STEP_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};

}  // namespace sts2
