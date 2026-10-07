// Models/RelicModel.cs
#include "game/Models/RelicModel.h"

#include "game/All.h"

namespace sts2 {

Player RelicModel::Owner() const { return st().owner ? Player(sim_, st().owner) : Player{}; }

int32_t RelicModel::MerchantCost_Default() const {
    switch (Rarity()) {
    case RelicRarity::Common: return 175;
    case RelicRarity::Uncommon: return 225;
    case RelicRarity::Rare: return 275;
    case RelicRarity::Shop: return 200;
    case RelicRarity::Ancient: return 999999999;
    case RelicRarity::Starter: return 999999999;
    case RelicRarity::Event: return 999999999;
    case RelicRarity::None: return 1;
    default: throw GameError("Relic has invalid merchant rarity.");
    }
}

bool RelicModel::IsBeforeAct3TreasureChest(RunState runState) {
    int32_t num = runState.Players().size() > 1 ? 38 : 41;
    return runState.TotalFloor() < num;
}

bool RelicModel::IsAllowedAtNeow_Default(Player player) const { return IsAllowed(player.RunState()); }

}  // namespace sts2
