// Models/Relics/MummifiedHand.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Entities/Cards/CardPile.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct MummifiedHand : RelicModel {
    SIM_MODEL(MummifiedHand, RelicModel)
    static bool CostsResources(CardModel card) {
        return (!card.EnergyCost().CostsX() && card.EnergyCost().GetWithModifiers(CostModifiers::All) > 0) ||
               (!card.HasStarCostX() && card.GetStarCostWithModifiers() > 0);
    }
    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || !CombatManager(sim_).IsInProgress() || play->Card.Owner() != Owner() ||
            play->Card.Type() != CardType::Power) return;
        CardList hand = CardPile::Get(PileType::Hand, Owner()).Cards();
        CardList preferred, preferredCostly, costly;
        for (CardModel card : hand) {
            bool baseCostly = card.EnergyCost().GetWithModifiers(CostModifiers::None) > 0 || card.BaseStarCost() > 0;
            if (baseCostly) preferred.push_back(card);
            if (CostsResources(card)) {
                costly.push_back(card);
                if (baseCostly) preferredCostly.push_back(card);
            }
        }
        Rng& rng = Owner().RunState().Rng().CombatCardSelection();
        CardModel chosen = rng.NextItem(preferredCostly);
        if (!chosen) chosen = rng.NextItem(costly);
        if (!chosen) chosen = rng.NextItem(preferred);
        if (!chosen) chosen = rng.NextItem(hand);
        if (chosen) chosen.SetToFreeThisTurn();
    }
};

}  // namespace sts2
