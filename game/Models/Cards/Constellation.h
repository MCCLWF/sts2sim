#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Constellation : CardModel {
    SIM_MODEL(Constellation, CardModel)
    int32_t CanonicalStarCost() const { return 2; }
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        Player target = play->Target.Player();
        if (!target) throw GameError("cardPlay.Target has no player");
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), target);
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), target);
        CreatureCmd::GainBlock(target.Creature(), DynamicVars().Block(), play);
    }
};
}  // namespace sts2
