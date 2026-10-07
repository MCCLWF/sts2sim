// Models/Cards/Null.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/OrbCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Null : CardModel { SIM_MODEL(Null, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, play->Target,
                        DynamicVars().Weak().BaseValue(), Owner().Creature(), *this);
        OrbCmd::Channel(OrbId::DARK_ORB, ctx, Owner());
    }
};
}
