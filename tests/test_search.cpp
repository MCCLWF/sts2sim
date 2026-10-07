#include "env/env.h"
#include "search/reference_ismcts.h"

using namespace sts2;

int main() {
    Env env;
    env.Reset(NewRunOptions{.seed = "M3SEARCH", .character = CharacterId::IRONCLAD, .ascension = 0});
    const SearchConfig config{.iterations = 12, .rollout_depth = 4, .seed = 99};
    const auto source_hash = env.Hash();
    const Action first = ReferenceIsmcts::Choose(env, config);
    const Action second = ReferenceIsmcts::Choose(env, config);
    if (!(first == second)) return 1;
    const auto legal = env.LegalActions();
    bool first_legal = false;
    for (const Action& action : legal) {
        first_legal |= action == first;
    }
    if (first_legal && env.Hash() == source_hash) return 0;
    return 2;
}
