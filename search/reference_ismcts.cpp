#include "search/reference_ismcts.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace sts2 {

namespace {

double score(const Env& env) {
    const auto observation = env.Observe();
    // Observe's first fields are schema, decision, act, floor, total floor,
    // phase, hp, max hp, block and energy. This is a deliberately generic
    // rollout value, not a gameplay policy.
    double value = static_cast<double>(observation[6]);
    value += 0.1 * static_cast<double>(observation[11]);  // gold
    value += 0.01 * static_cast<double>(observation[4]);  // progress
    if (env.decision().kind == DecisionKind::Ended && observation[6] <= 0) value -= 1000.0;
    return value;
}

Action random_action(Env& env, std::mt19937_64& rng) {
    const auto actions = env.LegalActions();
    if (actions.empty()) throw GameError("ReferenceIsmcts: no legal action");
    std::uniform_int_distribution<size_t> pick(0, actions.size() - 1);
    return actions[pick(rng)];
}

double rollout(const Env& root, const Action& first, int depth, std::mt19937_64& rng) {
    auto state = root.Predict(first);
    for (int step = 0; step < depth && state->decision().kind != DecisionKind::Ended; ++step) {
        state = state->Predict(random_action(*state, rng));
    }
    return score(*state);
}

}  // namespace

Action ReferenceIsmcts::Choose(const Env& root, const SearchConfig& config) {
    const auto actions = root.LegalActions();
    if (actions.empty()) throw GameError("ReferenceIsmcts: root has no legal action");
    if (actions.size() == 1 || config.iterations <= 0) return actions.front();

    std::mt19937_64 rng(config.seed);
    std::vector<int> visits(actions.size(), 0);
    std::vector<double> values(actions.size(), 0.0);
    for (int iteration = 0; iteration < config.iterations; ++iteration) {
        size_t selected = 0;
        double best = -std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < actions.size(); ++i) {
            double ucb;
            if (visits[i] == 0) {
                ucb = std::numeric_limits<double>::infinity();
            } else {
                const double mean = values[i] / static_cast<double>(visits[i]);
                const double exploration = 1.4142135623730951 *
                    std::sqrt(std::log(static_cast<double>(iteration + 1)) / visits[i]);
                ucb = mean + exploration;
            }
            if (ucb > best) {
                best = ucb;
                selected = i;
            }
        }
        double value;
        try {
            value = rollout(root, actions[selected], config.rollout_depth, rng);
        } catch (const Unported&) {
            throw;
        } catch (const GameError&) {
            value = -std::numeric_limits<double>::max() / 4.0;
        }
        ++visits[selected];
        values[selected] += value;
    }

    size_t best = 0;
    for (size_t i = 1; i < actions.size(); ++i) {
        const double lhs = visits[i] ? values[i] / visits[i] : -std::numeric_limits<double>::infinity();
        const double rhs = visits[best] ? values[best] / visits[best] : -std::numeric_limits<double>::infinity();
        if (lhs > rhs) best = i;
    }
    return actions[best];
}

}  // namespace sts2
