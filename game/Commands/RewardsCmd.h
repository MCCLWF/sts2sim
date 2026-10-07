// Commands/RewardsCmd.cs (OfferCustom).
//
// A rewards screen offered from inside an effect (an event option, a rest site
// heal, a relic's AfterObtained) is awaited by the effect: the code after
// OfferCustom runs once the player proceeds. Here that screen is a nested
// choice (Sim::Choose, source "rewards"): answer I < N takes reward I of the
// set (N = its size, taken rewards included), answer N proceeds. The rewards
// screen after a combat is a top-level decision instead (RunPhase::Rewards):
// nothing runs after it.
#pragma once

#include <span>

#include "game/All.h"

namespace sts2::RewardsCmd {

// The set a custom offer starts with: empty; the caller adds its rewards
// (WithCustomRewards) before offering it.
RewardsSet NewCustomSet(Sim* sim);
// RewardsSet.Offer, then the screen until the player proceeds.
void OfferAndAwait(RewardsSet set);
// OfferCustom(player, rewards)
void OfferCustom(Player player, std::span<const Reward> rewards);

}  // namespace sts2::RewardsCmd
