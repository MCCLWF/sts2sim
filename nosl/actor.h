// The deployed actor on observation tables alone: the actor filter, the
// native forward pass and the action draw. It needs no Env: the caller hands
// it the three tables of nosl.obs v1 and the legal actions they describe.
// nosl::Pilot (a shadow Env) and the game-side encoder (tools/NoslPilot) both
// decide through this one object, so equal tables give equal actions.
#pragma once

#include <cstdint>
#include <memory>
#include <random>
#include <string>

#include "nosl/actor_filter.h"
#include "nosl/observation.h"
#include "nosl/policy_net.h"

namespace sts2::nosl {

class Actor {
public:
    Actor(const std::string& weights, int32_t actor_filter, uint64_t policy_seed, bool greedy);

    struct Decision {
        Action action;      // the action taken
        size_t row = 0;     // its row in the filtered table
        size_t rows = 0;    // rows left by the filter
        bool forced = false;  // one row left: no network, no draw
        double prob = 1.0;
        float value = 0;
    };
    // Prunes `obs` with the filter (in place), chooses, and records the choice for the filter.
    Decision Decide(Observation& obs);

private:
    ActorFilter filter_;
    std::unique_ptr<PolicyNet> net_;
    std::mt19937_64 rng_;
    bool greedy_;
};

}  // namespace sts2::nosl
