// Models/Relics/LuckyFysh.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct LuckyFysh : RelicModel {
    SIM_MODEL(LuckyFysh, RelicModel)
    bool IsAllowedInShops() const { return false; }
    bool IsAllowed(RunState runState) const { return IsBeforeAct3TreasureChest(runState); }
    void AfterCardChangedPiles(CardModel card, PileType, AbstractModel) const {
        if (card.Owner() == Owner() && card.Pile() && card.Pile().Type() == PileType::Deck)
            PlayerCmd::GainGold(DynamicVars().Gold().BaseValue(), Owner());
    }
};

}  // namespace sts2
