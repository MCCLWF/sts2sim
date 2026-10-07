// Models/Relics/BookOfFiveRings.cs (IsAllowed only: the relic grab bag asks every relic)
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct BookOfFiveRings : RelicModel {
    SIM_MODEL(BookOfFiveRings, RelicModel)
    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return extra(0) % DynamicVars().Cards().IntValue(); }

    void AfterCardChangedPiles(CardModel card, PileType, AbstractModel) const {
        if (Owner().Creature().IsDead() || card.Owner() != Owner()) return;
        CardPile pile = card.Pile();
        if (!pile || pile.Type() != PileType::Deck) return;
        ++extra(0);
        if (extra(0) % DynamicVars().Cards().IntValue() == 0)
            CreatureCmd::Heal(Owner().Creature(), DynamicVars().Heal().BaseValue());
    }

    bool IsAllowed(RunState runState) const {
        return IsBeforeAct3TreasureChest(runState);
    }
};

}  // namespace sts2
