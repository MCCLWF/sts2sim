// Offline only: replace on a combat's Env everything the player cannot see.
//
// The order of the draw and discard piles (Env::Determinize) and every random
// stream a combat draws from (the shuffle, card generation, targets, enemy
// moves, each monster's own) are set from `seed`. What the player sees, and
// therefore the actor observation, does not change. Two limits: a card
// publicly known to be on top of the draw pile is resampled away with the
// rest; and it cannot be done while a choice inside an effect is pending
// (Determinize takes the snapshot such a choice replays from).
//
// Used by the rollout tools (tools/nosl/rollout.h) and by the training
// sampler's combat arena (nosl_batch_resample). Nothing deployed calls it.
#pragma once

#include <cstdint>

#include "env/env.h"

namespace sts2::nosl {

void ResampleHidden(Env& world, uint64_t seed);

}  // namespace sts2::nosl
