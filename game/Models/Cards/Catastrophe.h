#pragma once

#include "core/dotnet_sort.h"
#include "game/Commands/CardCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Catastrophe : CardModel {
    SIM_MODEL(Catastrophe, CardModel)

    static CardModel RandomDrawCard(Player owner, bool playableOnly) {
        CardList candidates;
        for (CardModel card : owner.PlayerCombatState().DrawPile().Cards()) {
            if (!playableOnly || !card.Keywords().Contains(CardKeyword::Unplayable)) candidates.push_back(card);
        }
        if (candidates.empty()) return {};
        dotnet_sort(candidates.span(), [](CardModel a, CardModel b) { return a.CompareTo(b); });
        owner.RunState().Rng().Shuffle().Shuffle(candidates);
        return candidates.front();
    }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i) {
            CardModel card = RandomDrawCard(Owner(), true);
            if (!card) card = RandomDrawCard(Owner(), false);
            if (!card) break;
            CardCmd::AutoPlay(ctx, card, Creature{});
        }
    }

};

}  // namespace sts2
