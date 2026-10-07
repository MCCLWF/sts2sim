#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Glow : CardModel {
    SIM_MODEL(Glow, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
        PowerCmd::Apply(PowerId::DRAW_CARDS_NEXT_TURN_POWER, ctx, Owner().Creature(), DynamicVars().Cards().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
