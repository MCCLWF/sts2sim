#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct DrainPower : CardModel {
    SIM_MODEL(DrainPower, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardList candidates;
        for (CardModel card : Owner().PlayerCombatState().DiscardPile().Cards())
            if (card.IsUpgradable()) candidates.push_back(card);
        Owner().RunState().Rng().CombatCardSelection().Shuffle(candidates);
        int count = std::min(DynamicVars().Cards().IntValue(), static_cast<int>(candidates.size()));
        for (int i = 0; i < count; ++i) CardCmd::Upgrade(candidates[static_cast<size_t>(i)]);
    }
};

}  // namespace sts2
