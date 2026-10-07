#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct GuidingStar : CardModel {
    SIM_MODEL(GuidingStar, CardModel)
    int32_t CanonicalStarCost() const { return 1; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        PowerCmd::Apply(PowerId::DRAW_CARDS_NEXT_TURN_POWER, ctx, Owner().Creature(), DynamicVars().Cards().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
