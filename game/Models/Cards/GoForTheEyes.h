// Models/Cards/GoForTheEyes.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct GoForTheEyes : CardModel { SIM_MODEL(GoForTheEyes, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        if (play->Target.Monster().IntendsToAttack())
            PowerCmd::Apply(PowerId::WEAK_POWER, ctx, play->Target,
                            DynamicVars().Weak().BaseValue(), Owner().Creature(), *this);
    }
};
}
