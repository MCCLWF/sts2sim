// Models/Relics/BrilliantScarf.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/Entities/Cards/CardPile.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct BrilliantScarf : RelicModel {
    SIM_MODEL(BrilliantScarf, RelicModel)

    int32_t CardsPlayedThisTurn() const { return extra(0); }
    void SetCardsPlayedThisTurn(int32_t value) const {
        extra(0) = value;
        SetStatus(value == DynamicVars().Cards().IntValue() - 1 ? RelicStatus::Active : RelicStatus::Normal);
    }
    bool ShowCounter() const {
        return CombatManager(&sim()).IsInProgress() && CardsPlayedThisTurn() < DynamicVars().Cards().IntValue();
    }
    int32_t DisplayAmount() const { return CardsPlayedThisTurn(); }

    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature())) SetCardsPlayedThisTurn(0);
    }
    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!CombatManager(&sim()).IsInProgress() || !play || play->IsAutoPlay || play->Card.Owner() != Owner()) return;
        SetCardsPlayedThisTurn(CardsPlayedThisTurn() + 1);
    }
    void AfterCombatEnd(CombatRoom) const { SetCardsPlayedThisTurn(0); }
    bool TryModifyEnergyCostInCombatLate(CardModel card, Dec originalCost, Dec& modifiedCost) const {
        return TryModifyCost(card, originalCost, modifiedCost);
    }
    bool TryModifyStarCost(CardModel card, Dec originalCost, Dec& modifiedCost) const {
        return TryModifyCost(card, originalCost, modifiedCost);
    }

private:
    bool TryModifyCost(CardModel card, Dec originalCost, Dec& modifiedCost) const {
        modifiedCost = originalCost;
        if (!CombatManager(&sim()).IsInProgress() || card.Owner().Creature() != Owner().Creature() ||
            CardsPlayedThisTurn() != DynamicVars().Cards().IntValue() - 1) return false;
        CardPile pile = card.Pile();
        if (!pile || (pile.Type() != PileType::Hand && pile.Type() != PileType::Play)) return false;
        modifiedCost = Dec(0);
        return true;
    }
};

}  // namespace sts2
