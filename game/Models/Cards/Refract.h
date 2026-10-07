// Models/Cards/Refract.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Refract : CardModel { SIM_MODEL(Refract, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(2)
            .FromCard(*this, play).OnlyPlayAnimOnce().Targeting(play->Target).Execute(ctx);
        for (int i = 0; i < DynamicVars().Repeat().IntValue(); ++i)
            OrbCmd::Channel(OrbId::GLASS_ORB, ctx, Owner());
    }
};
}
