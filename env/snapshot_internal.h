#pragma once
#include <string>
#include "engine/sim/state.h"
namespace sts2::snapshot_detail {
const std::string& runtime_id();
void validate_game_state(const GameState& state);
}
