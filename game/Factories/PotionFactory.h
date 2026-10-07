// Factories/PotionFactory.cs. Potions come back canonical (ids); the caller
// makes the mutable instance (ToMutable) as C# does.
#pragma once

#include "game/All.h"

namespace sts2::PotionFactory {

PotionId CreateRandomPotionOutOfCombat(Player player, Rng& rng);
StaticVec<PotionId, 8> CreateRandomPotionsOutOfCombat(Player player, int32_t count, Rng& rng);
PotionId CreateRandomPotionInCombat(Player player, Rng& rng);
StaticVec<PotionId, 96> GetPotionOptions(Player player);

}  // namespace sts2::PotionFactory
