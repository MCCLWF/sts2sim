// Models/Cards/BeatIntoShape.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/ForgeCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct BeatIntoShape : CardModel {
    SIM_MODEL(BeatIntoShape, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("BeatIntoShape: cardPlay.Target is null");
        AttackCommand attack = DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target);
        attack.Execute(ctx);
        const Dec calculatedForge = DynamicVars()["CalculatedForge"].Calculate(play->Target);
        const Dec netForge = calculatedForge - DynamicVars().CalculationExtra().BaseValue() * Dec(static_cast<int32_t>(attack.Results().size()));
        ForgeCmd::Forge(netForge, Owner(), *this);
    }
};
}  // namespace sts2
