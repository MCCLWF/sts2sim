#pragma once

#include <cstdint>
#include <vector>

#include "env/env.h"

namespace sts2 {

struct SearchConfig {
    int iterations = 64;
    int rollout_depth = 32;
    int beam_width = 8;
    int node_budget = 512;
    uint64_t seed = 1;
};

// Small, dependency-free search reference used to validate the environment
// contract. It is intentionally conservative: each rollout owns a deep Env
// clone, so the implementation never mutates the caller's state.
class ReferenceIsmcts {
public:
    static Action Choose(const Env& root, const SearchConfig& config = {});
};

// Deterministic depth-limited beam search. Its candidate expansion and beam
// retention are intentionally separate from the reference ISMCTS so the game
// bridge can compare a Beam-shaped Env backend with CombatSolver's native
// Beam solver without calling the two algorithms equivalent by accident.
class ReferenceBeam {
public:
    static std::vector<Action> ChoosePath(const Env& root, const SearchConfig& config = {});
    static Action Choose(const Env& root, const SearchConfig& config = {});
};

}  // namespace sts2
