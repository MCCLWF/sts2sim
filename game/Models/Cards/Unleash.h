#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Unleash : CardModel {
    SIM_MODEL(Unleash, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        Creature osty = Owner().PlayerCombatState().GetPet(MonsterId::OSTY);
        if (!osty || !osty.IsAlive()) return;
        DamageCmd::Attack(DynamicVars().CalculatedDamage()).FromOsty(osty, *this, play).Targeting(play->Target).Execute(ctx);
    }
};

}  // namespace sts2
