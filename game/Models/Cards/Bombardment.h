#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Bombardment : CardModel {
    SIM_MODEL(Bombardment, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
    void AfterAutoPrePlayPhaseEnteredEarly(PlayerChoiceContext& ctx, Player player) const {
        if (Pile() && Pile().Type() == PileType::Exhaust && player == Owner()) CardCmd::AutoPlay(ctx, *this, Creature{});
    }
};
}  // namespace sts2
