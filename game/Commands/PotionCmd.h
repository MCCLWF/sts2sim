// Commands/PotionCmd.cs (the parts ported so far).
#pragma once

#include "game/All.h"

namespace sts2::PotionCmd {

Player::PotionProcureResult TryToProcure(PotionModel potion, Player player, int32_t slotIndex = -1);
// TryToProcure<T>: a new instance of `potion`.
Player::PotionProcureResult TryToProcure(PotionId potion, Player player, int32_t slotIndex = -1);
void Discard(PotionModel potion);

}  // namespace sts2::PotionCmd
