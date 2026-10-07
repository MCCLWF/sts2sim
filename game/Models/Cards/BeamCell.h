// Models/Cards/BeamCell.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct BeamCell : CardModel { SIM_MODEL(BeamCell, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, play->Target,
                        DynamicVars().Vulnerable().BaseValue(), Owner().Creature(), *this);
    }
};
}
