// Odds/PotionRewardOdds.cs. CurrentValue lives in PlayerData.odds.potion_reward;
// the Rng is the player's Rewards stream (PlayerOddsSet).
#pragma once

#include "game/All.h"

namespace sts2::PotionRewardOdds {

bool Roll(Player player, RoomType roomType);

}  // namespace sts2::PotionRewardOdds
