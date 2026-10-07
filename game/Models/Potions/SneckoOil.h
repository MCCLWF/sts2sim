#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct SneckoOil : PotionModel {
    SIM_MODEL(SneckoOil, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target) return;
        Player player = target.Player();
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), player);
        for (CardModel card : player.PlayerCombatState().Hand().Cards()) {
            if (!card.EnergyCost().CostsX() && card.EnergyCost().GetWithModifiers(CostModifiers::None) >= 0)
                card.EnergyCost().SetThisTurnOrUntilPlayed(player.RunState().Rng().CombatEnergyCosts().NextInt(4));
        }
    }
};

}  // namespace sts2
