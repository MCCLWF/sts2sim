// Models/Cards/Sunder.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Sunder : CardModel { SIM_MODEL(Sunder, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        AttackCommand attack = DamageCmd::Attack(DynamicVars().Damage().BaseValue());
        attack.FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        for (const auto& hits : attack.Results()) for (const DamageResult& result : hits)
            if (result.WasTargetKilled) {
                PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
                return;
            }
    }
};
}
