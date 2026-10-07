#pragma once

#include <cstdint>
#include <vector>

#include "env/env.h"

namespace sts2 {

struct SearchConfig {
    int iterations = 64;
    int rollout_depth = 32;
    uint64_t seed = 1;
};

// Small, dependency-free search reference used to validate the environment
// contract. It is intentionally conservative: each rollout owns a deep Env
// clone, so the implementation never mutates the caller's state.
class ReferenceIsmcts {
public:
    static Action Choose(const Env& root, const SearchConfig& config = {});
};

}  // namespace sts2
