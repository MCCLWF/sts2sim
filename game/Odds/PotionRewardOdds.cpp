// Odds/PotionRewardOdds.cs
#include "game/Odds/PotionRewardOdds.h"

#include "game/Hooks/Hook.h"

namespace sts2::PotionRewardOdds {

bool Roll(Player player, RoomType roomType) {
    float& value = player.st().odds.potion_reward;
    float currentValue = value;
    if (Hook::ShouldForcePotionReward(player.RunState(), player, roomType)) return true;
    float num = roomType != RoomType::Elite ? 0.0f : 0.25f;  // eliteBonus
    float num2 = num;
    float num3 = currentValue + num2 * 0.5f;
    float num4 = player.PlayerRng().Rewards().NextFloat();
    if (num4 < num3) {
        value -= 0.1f;
        return true;
    }
    value += 0.1f;
    return false;
}

}  // namespace sts2::PotionRewardOdds
