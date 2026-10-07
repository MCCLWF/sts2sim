#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Anointed : CardModel {
    SIM_MODEL(Anointed, CardModel)

    bool CanBeGeneratedInCombat() const { return true; }

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        int32_t count = CardPile::MaxCardsInHand - Owner().PlayerCombatState().Hand().Count();
        if (count <= 0) return;
        CardList candidates;
        for (CardModel card : Owner().PlayerCombatState().DrawPile().Cards()) {
            if (card.Rarity() == CardRarity::Rare) candidates.push_back(card);
        }
        Owner().RunState().Rng().CombatCardSelection().Shuffle(candidates);
        CardList selected;
        for (int32_t i = 0; i < count && i < static_cast<int32_t>(candidates.size()); ++i) selected.push_back(candidates[static_cast<size_t>(i)]);
        CardPileCmd::Add(selected, PileType::Hand);
    }

};

}  // namespace sts2
