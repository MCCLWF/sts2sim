#include "env/env.h"
#include "search/reference_ismcts.h"

using namespace sts2;

int main() {
    Env env;
    env.Reset(NewRunOptions{.seed = "M3SEARCH", .character = CharacterId::IRONCLAD, .ascension = 0});
    const SearchConfig config{.iterations = 12, .rollout_depth = 4, .seed = 99};
    const Action first = ReferenceIsmcts::Choose(env, config);
    const Action second = ReferenceIsmcts::Choose(env, config);
    if (!(first == second)) return 1;
    const SearchConfig beam_config{.rollout_depth = 4, .beam_width = 3, .node_budget = 24};
    const Action beam_first = ReferenceBeam::Choose(env, beam_config);
    const Action beam_second = ReferenceBeam::Choose(env, beam_config);
    if (!(beam_first == beam_second)) return 3;
    const auto legal = env.LegalActions();
    bool first_legal = false;
    bool beam_legal = false;
    for (const Action& action : legal) {
        first_legal |= action == first;
        beam_legal |= action == beam_first;
    }
    if (first_legal && beam_legal) return 0;
    return 2;
}
