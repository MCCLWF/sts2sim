// Models/Cards/BallLightning.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct BallLightning : CardModel { SIM_MODEL(BallLightning, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        OrbCmd::Channel(OrbId::LIGHTNING_ORB, ctx, Owner());
    }
};
}
