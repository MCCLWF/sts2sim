// Models/Cards/Whistle.cs (v0.111.0).
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Whistle : CardModel {
    SIM_MODEL(Whistle, CardModel)

    void OnPlay(PlayerChoiceContext& context, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .FromCard(*this, play).Targeting(play->Target)
            .WithHitFx("vfx/vfx_attack_slash")
            .Execute(context);
        CreatureCmd::Stun(play->Target);
    }
};

}  // namespace sts2
