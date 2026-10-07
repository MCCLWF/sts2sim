#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/PotionModel.h"

namespace sts2 {
struct CosmicConcoction : PotionModel {
    SIM_MODEL(CosmicConcoction, PotionModel)
    void OnUse(PlayerChoiceContext&, Creature target) const {
        if (!target || !target.Player()) throw GameError("CosmicConcoction requires a player target");
        Player player = target.Player();
        CardList cards = CardFactory::GetForCombat(player, CardPoolId::COLORLESS_CARD_POOL,
            DynamicVars().Cards().IntValue(), player.RunState().Rng().CombatCardGeneration(), CardType::None, true);
        for (CardModel card : cards) {
            CardCmd::Upgrade(card);
            CardPileCmd::AddGeneratedCardToCombat(card, PileType::Hand, Owner());
        }
    }
};
}  // namespace sts2
