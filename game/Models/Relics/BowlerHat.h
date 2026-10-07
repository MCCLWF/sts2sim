// Models/Relics/BowlerHat.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"
namespace sts2 {
struct BowlerHat : RelicModel {
    SIM_MODEL(BowlerHat, RelicModel)
    bool IsAllowedInShops() const { return false; }
    Dec ModifyGoldGained(Player player, Dec amount) const {
        return player == Owner() ? amount * DynamicVars()["GoldIncrease"].BaseValue() : amount;
    }
    void AfterModifyingGoldGained(Player, Dec) const { Flash(); }
    bool IsAllowed(RunState runState) const { return IsBeforeAct3TreasureChest(runState); }
};
}
