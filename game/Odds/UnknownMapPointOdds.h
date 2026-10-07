// Odds/UnknownMapPointOdds.cs. The non-event odds live in RunData.odds; the
// base odds are the constants (only the DeadlyEvents modifier changes them).
// The Rng is RunRngSet.UnknownMapPoint (RunOddsSet).
#pragma once

#include "game/All.h"

namespace sts2::UnknownMapPointOddsOps {

// Roll(blacklist, runState): the room type of an unknown map point.
RoomType Roll(Sim* sim, uint16_t blacklist);  // bit per RoomType
float EventOdds(const Sim& sim);

}  // namespace sts2::UnknownMapPointOddsOps
