// GameActions/ActionExecutor.cs with the player-driven combat actions
// (GameActions/PlayCardAction.cs, GameActions/EndPlayerTurnAction.cs). The
// queue is gone: each call runs one action to completion, then what the
// executor does after it, then the turn loop up to the player's next wait.
#pragma once

#include "game/All.h"

namespace sts2::ActionExecutor {

// PlayCardAction(card, target). False when the action cancels itself (the
// card is not in the hand, cannot be played, or the target is invalid).
bool PlayCard(Sim* sim, CardModel card, Creature target);

// EndPlayerTurnAction(player, turnNumber): the turn number must be the
// player's current one.
void EndTurn(Sim* sim, int32_t turnNumber);

}  // namespace sts2::ActionExecutor
