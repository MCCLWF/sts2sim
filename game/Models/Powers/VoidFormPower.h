// Models/Powers/VoidFormPower.cs
#pragma once

#include "game/Entities/Cards/CardPlay.h"
#include "game/Entities/Cards/CardPile.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct VoidFormPower : PowerModel {
    SIM_MODEL(VoidFormPower, PowerModel)

    int32_t CardsPlayedThisTurn() const { return extra(0); }
    void SetCardsPlayedThisTurn(int32_t value) const { extra(0) = value; }

    // NVoidFormVfx only creates/activates a visual node. It has no gameplay
    // reads; its counter-changing helper below must still run.
    void AfterApplied(Creature, CardModel) const {}
    void AfterRemoved(Creature) const {}

    void BeforePowerAmountChanged(PowerModel power, Dec, Creature, Creature, CardModel) const {
        if (power != *this) return;
        HideTemporaryZeroCostVisual();
    }

    void BeforeApplied(Creature, Dec, Creature, CardModel) const {
        HideTemporaryZeroCostVisual();
    }

    bool TryModifyEnergyCostInCombatLate(CardModel card, Dec originalCost, Dec& modifiedCost) const {
        modifiedCost = originalCost;
        if (ShouldSkip(card)) return false;
        modifiedCost = Dec(0);
        return true;
    }

    bool TryModifyStarCost(CardModel card, Dec originalCost, Dec& modifiedCost) const {
        modifiedCost = originalCost;
        if (ShouldSkip(card)) return false;
        modifiedCost = Dec(0);
        return true;
    }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* cardPlay) const {
        if (cardPlay && cardPlay->Card.Owner().Creature() == Owner()
            && !cardPlay->IsAutoPlay && cardPlay->IsLastInSeries()) {
            SetCardsPlayedThisTurn(CardsPlayedThisTurn() + 1);
        }
        // The remaining native branch only disables Vfx after the threshold.
    }

    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants,
                             sts2::CombatState) const {
        if (!participants.contains(Owner())) return;
        SetCardsPlayedThisTurn(0);
    }

private:
    bool ShouldSkip(CardModel card) const {
        if (card.Owner().Creature() != Owner()) return true;
        CardPile pile = card.Pile();
        if (!pile || (pile.Type() != PileType::Hand && pile.Type() != PileType::Play)) return true;
        return CardsPlayedThisTurn() >= Amount();
    }

    void HideTemporaryZeroCostVisual() const {
        // Despite its name, this is gameplay cost state in the shipped model.
        SetCardsPlayedThisTurn(999999999);
    }
};

}  // namespace sts2
