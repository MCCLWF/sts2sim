// Models/Relics/Crossbow.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Crossbow : RelicModel {
    SIM_MODEL(Crossbow, RelicModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature())) return;
        CardList attacks = CardFactory::GetForCombat(
            Owner(), sts2::data(Owner().Character()).card_pool, 1, Owner().RunState().Rng().CombatCardGeneration(),
            CardType::Attack, true);
        for (CardModel card : attacks) {
            card.SetToFreeThisTurn();
            CardPileCmd::AddGeneratedCardToCombat(card, PileType::Hand, Owner());
        }
    }
};

}  // namespace sts2
