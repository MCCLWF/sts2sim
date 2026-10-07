// Models/Relics/OldCoin.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct OldCoin : RelicModel {
    SIM_MODEL(OldCoin, RelicModel)
    bool IsAllowedInShops() const { return false; }
    bool HasUponPickupEffect() const { return true; }
    bool IsAllowed(RunState runState) const { return IsBeforeAct3TreasureChest(runState); }
    void AfterObtained() const { PlayerCmd::GainGold(DynamicVars().Gold().BaseValue(), Owner()); }
};

}  // namespace sts2
