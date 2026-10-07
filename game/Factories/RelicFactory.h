// Factories/RelicFactory.cs. Relics come back canonical (ids); the caller
// makes the mutable instance (ToMutable) as C# does. TestRngInjector
// overrides are test-only and never set.
#pragma once

#include "game/All.h"
#include "game/Runs/RelicGrabBag.h"

namespace sts2::RelicFactory {

inline constexpr RelicId FallbackRelic = RelicId::CIRCLET;

RelicId PullNextRelicFromFront(Player player, Rng& rng);
RelicId PullNextRelicFromFront(Player player);
RelicId PullNextRelicFromFront(Player player, RelicRarity rarity, const RelicGrabBagOps::RelicFilter& filter = nullptr);
RelicId PullNextRelicFromBack(Player player);
RelicId PullNextRelicFromBack(Player player, RelicRarity rarity, const RelicGrabBagOps::RelicFilter& filter = nullptr);
RelicRarity RollRarity(Player player);
RelicRarity RollRarity(Rng& rng);

}  // namespace sts2::RelicFactory
