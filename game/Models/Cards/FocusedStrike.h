// Models/Cards/FocusedStrike.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct FocusedStrike : CardModel { SIM_MODEL(FocusedStrike, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        PowerCmd::Apply(PowerId::FOCUSED_STRIKE_POWER, ctx, Owner().Creature(),
                        DynamicVars()["FocusPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
