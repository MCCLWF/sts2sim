// Unlocks/UnlockState.cs. The simulator always plays on the fully unlocked
// profile (UnlockState.all, AGENTS.md §4): every epoch revealed, every
// encounter seen.
#pragma once

#include <cstdint>

namespace sts2::UnlockState {

inline constexpr int32_t NumberOfRuns = 999999999;  // UnlockState.all

}  // namespace sts2::UnlockState
