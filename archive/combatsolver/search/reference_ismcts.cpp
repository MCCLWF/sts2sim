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

namespace {

struct BeamNode {
    std::unique_ptr<Env> state;
    std::vector<Action> path;
    double value = 0;
};

bool action_less(const Action& a, const Action& b) {
    if (a.kind != b.kind) return a.kind < b.kind;
    if (a.index != b.index) return a.index < b.index;
    if (a.target != b.target) return a.target < b.target;
    if (a.coord.col != b.coord.col) return a.coord.col < b.coord.col;
    return a.coord.row < b.coord.row;
}

bool node_better(const BeamNode& a, const BeamNode& b) {
    if (a.value != b.value) return a.value > b.value;
    return std::lexicographical_compare(a.path.begin(), a.path.end(),
                                        b.path.begin(), b.path.end(), action_less);
}

}  // namespace

std::vector<Action> ReferenceBeam::ChoosePath(const Env& root, const SearchConfig& config) {
    const auto root_actions = root.LegalActions();
    if (root_actions.empty()) throw GameError("ReferenceBeam: no legal action");
    if (config.node_budget <= 0 || config.beam_width <= 0 || config.rollout_depth <= 0)
        return {root_actions.front()};

    std::vector<BeamNode> frontier;
    frontier.push_back(BeamNode{root.Clone(), {}, score(root)});
    BeamNode best = BeamNode{root.Clone(), {}, score(root)};
    int expanded = 0;
    for (int depth = 0; depth < config.rollout_depth && !frontier.empty(); ++depth) {
        std::vector<BeamNode> next;
        for (BeamNode& parent : frontier) {
            for (const Action& action : parent.state->LegalActions()) {
                if (expanded >= config.node_budget) break;
                ++expanded;
                auto child = parent.state->Predict(action);
                BeamNode candidate{std::move(child), parent.path, 0};
                candidate.path.push_back(action);
                candidate.value = score(*candidate.state);
                if (node_better(candidate, best)) best = BeamNode{candidate.state->Clone(), candidate.path, candidate.value};
                next.push_back(std::move(candidate));
            }
            if (expanded >= config.node_budget) break;
        }
        if (next.empty()) break;
        std::stable_sort(next.begin(), next.end(), node_better);
        if (next.size() > static_cast<size_t>(config.beam_width)) next.resize(static_cast<size_t>(config.beam_width));
        frontier = std::move(next);
        if (expanded >= config.node_budget) break;
    }
    return best.path.empty() ? std::vector<Action>{root_actions.front()} : best.path;
}

Action ReferenceBeam::Choose(const Env& root, const SearchConfig& config) {
    return ChoosePath(root, config).front();
}

}  // namespace sts2
