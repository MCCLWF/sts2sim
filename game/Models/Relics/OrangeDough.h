#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/RelicModel.h"

namespace sts2 {
struct OrangeDough : RelicModel {
    SIM_MODEL(OrangeDough, RelicModel)
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        Player owner = Owner();
        if (!participants.contains(owner.Creature()) || owner.PlayerCombatState().TurnNumber() > 1) return;
        CardList cards = CardFactory::GetForCombat(owner, CardPoolId::COLORLESS_CARD_POOL, DynamicVars().Cards().IntValue(),
            owner.RunState().Rng().CombatCardGeneration(), CardType::None, true);
        CardPileCmd::AddGeneratedCardsToCombat(cards, PileType::Hand, owner);
    }
};
}  // namespace sts2
