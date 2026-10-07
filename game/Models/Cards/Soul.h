#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Soul : CardModel {
    SIM_MODEL(Soul, CardModel)

    static CardList Create(Player owner, int32_t amount, sts2::CombatState combatState) {
        CardList cards;
        for (int32_t i = 0; i < amount; ++i) cards.push_back(combatState.CreateCard(CardId::SOUL, owner));
        return cards;
    }

    static CardList CreateInHand(Player owner, int32_t amount, sts2::CombatState combatState, Player creator = {}) {
        CardList cards = Create(owner, amount, combatState);
        CardPileCmd::AddGeneratedCardsToCombat(cards, PileType::Hand, creator ? creator : owner);
        return cards;
    }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
    }

};

}  // namespace sts2
