// Runs/RelicGrabBag.cs: the operations on the POD RelicGrabBag
// (engine/run_state.h). Its relics are canonical models; IsAllowed runs on a
// scratch instance (game/Models/Canonical.h).
#pragma once

#include <cstdint>
#include <functional>

#include "game/All.h"

namespace sts2::RelicGrabBagOps {

using RelicFilter = std::function<bool(RelicId relic)>;  // Func<RelicModel, bool>; empty = every relic

RelicId PullFromFront(Sim* sim, RelicGrabBag& bag, RelicRarity rarity, const RelicFilter& filter);  // NONE = null
RelicId PullFromBack(Sim* sim, RelicGrabBag& bag, RelicRarity rarity, const RelicFilter& filter);
void Remove(RelicGrabBag& bag, RelicId relic);  // from every deque
bool Contains(const RelicGrabBag& bag, RelicId relic);
bool HasAvailableRelics(Sim* sim, RelicGrabBag& bag);

}  // namespace sts2::RelicGrabBagOps
