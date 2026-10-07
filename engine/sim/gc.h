// Pool reclamation. The C# heap is garbage collected; here objects live in
// fixed pools, so unreachable slots are freed by a mark-and-sweep over the
// state. The full sweep runs at quiescent points outside combat (after leaving
// a room), where no handle lives outside the state and every live object is
// reachable from these roots:
//   - the deck, the relics and the potion slots;
//   - the player's creature and its powers;
//   - the combat piles (empty after combat) and the rewards set being offered.
// Content that keeps a reference in an `extra` slot (a bottled card, a
// remembered relic, ...) must add it to the roots here when it is ported.
#pragma once

#include "engine/sim/state.h"

namespace sts2 {

// Returns the number of slots freed. Does nothing while a combat exists.
int CollectGarbage(GameState& g);

// At a completed combat decision, reclaim detached creatures, retired powers,
// and AfflictionModels that no live state can still refer to. Retained card
// targets, power captures and model-specific references are roots. Never call
// while an action or choice is being executed. Returns the total slots freed.
int CollectDetachedCreatures(GameState& g);

}  // namespace sts2
