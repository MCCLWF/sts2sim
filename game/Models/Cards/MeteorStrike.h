#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct MeteorStrike : CardModel { SIM_MODEL(MeteorStrike, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        for (int32_t i = 0; i < 3; ++i) OrbCmd::Channel(OrbId::PLASMA_ORB, ctx, Owner());
    }
};
}
