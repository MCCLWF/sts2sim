#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct LiquidMemories : PotionModel {
    SIM_MODEL(LiquidMemories, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target) return;
        Player player = target.Player();
        CardList selected = CardSelectCmd::FromCombatPile(ctx, player.PlayerCombatState().DiscardPile(), player,
                                                           CardSelectorPrefs(1));
        if (selected.empty()) return;  // Original `FirstOrDefault()` is safe on an empty discard pile.
        CardModel card = selected.front();
        if (card) {
            card.SetToFreeThisTurn();
            CardPileCmd::Add(card, PileType::Hand);
        }
    }
};

}  // namespace sts2
