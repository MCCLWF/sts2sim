// Odds/CardRarityOdds.cs. CurrentValue lives in PlayerData.odds.card_rarity;
// the Rng is the player's Rewards stream (PlayerOddsSet).
#pragma once

#include "game/All.h"

namespace sts2::CardRarityOdds {

CardRarity Roll(Player player, CardRarityOddsType type);
CardRarity RollWithoutChangingFutureOdds(Player player, CardRarityOddsType type, float offset);
CardRarity RollWithBaseOdds(Player player, CardRarityOddsType type);

}  // namespace sts2::CardRarityOdds
