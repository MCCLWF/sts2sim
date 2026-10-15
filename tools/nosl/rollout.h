// What the offline rollout tools share (nosl_headroom --player rollout,
// nosl_rollout_labels): reading recorded runs, resampling what the player
// cannot see, and what one action is worth when the public policy finishes
// the combat after it.
//
// Offline only. A rollout steps copies of the Env; nothing here is part of a
// deployed policy.
#pragma once

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/rng.h"
#include "engine/sim/errors.h"
#include "game/All.h"
#include "nosl/actor_filter.h"
#include "nosl/observation.h"
#include "nosl/policy_net.h"
#include "nosl/resample.h"

namespace sts2::nosl::rollout {

// One line of an evaluation or collection file: {seed, ascension, actor_filter, action_rows}.
struct Trajectory { std::string seed; int ascension = 10, filter = 0; std::vector<int32_t> rows; };

inline bool Field(const std::string& line, const std::string& key, std::string& out) {
    const size_t at = line.find("\"" + key + "\":");
    if (at == std::string::npos) return false;
    size_t begin = at + key.size() + 3;
    while (begin < line.size() && line[begin] == ' ') ++begin;
    size_t end = begin;
    if (line[begin] == '[') end = line.find(']', begin) + 1;
    else if (line[begin] == '"') { ++begin; end = line.find('"', begin); }
    else end = line.find_first_of(",}", begin);
    out = line.substr(begin, end - begin);
    return true;
}

inline Trajectory Parse(const std::string& line) {
    Trajectory t;
    std::string value;
    if (!Field(line, "seed", t.seed) || !Field(line, "action_rows", value)) throw std::invalid_argument("trajectory line lacks seed or action_rows");
    for (char& c : value) if (c == ',' || c == '[' || c == ']') c = ' ';
    std::istringstream rows(value);
    for (int32_t row; rows >> row;) t.rows.push_back(row);
    if (Field(line, "ascension", value)) t.ascension = std::stoi(value);
    if (Field(line, "actor_filter", value)) t.filter = std::stoi(value);
    return t;
}

inline std::string FightKind(const std::string& encounter) {
    auto ends = [&](const char* suffix) {
        const std::string s(suffix);
        return encounter.size() >= s.size() && encounter.compare(encounter.size() - s.size(), s.size(), s) == 0;
    };
    if (ends("_BOSS")) return "boss";
    if (ends("_ELITE")) return "elite";
    if (ends("_WEAK")) return "weak";
    return "normal";
}

inline int32_t Hp(Env& env) { return env.state().creatures[env.state().run.player.creature].current_hp; }

// Resample on `world` everything the player cannot see (nosl/resample.h).
inline void Resample(Env& world, uint64_t seed) { ResampleHidden(world, seed); }

struct Options {
    int32_t samples = 8, top = 4;
    double z = 1.0, win = 3.0, lose = 1.0, potion_hp = 4.0;
};

// The policy's row at the observation of `env` behind `filter` (which is given the observation).
inline size_t GreedyRow(Env& env, ActorFilter& filter, const PolicyNet& net, Observation& obs, std::vector<float>& logits) {
    float value = 0;
    Observe(env, obs);
    filter.Apply(obs);
    if (obs.legal.empty()) throw GameError("no legal action");
    size_t row = 0;
    if (obs.legal.size() > 1) {
        net.Forward(obs, logits, value);
        for (size_t i = 1; i < logits.size(); ++i) if (logits[i] > logits[row]) row = i;
    }
    return row;
}

// What taking `first` and then following the policy to the end of the combat is worth on the copy `world`:
// `win` plus the fraction of maximum HP left, less `potion_hp` HP for each potion spent; after a death, `lose`
// times the fraction of the enemies' HP that was gone, so that a lost fight still ranks its actions.
inline double Worth(Env& world, ActorFilter filter, const Action& first, const PolicyNet& net, const Options& ro, int32_t max_hp) {
    Observation obs;
    std::vector<float> logits;
    int32_t potions = first.kind == Action::UsePotion;
    filter.OnAction(first);
    world.Step(first);
    for (int32_t step = 0;; ++step) {
        if (world.decision().kind == DecisionKind::Ended || !world.state().combat.is_in_progress) break;
        if (step >= 1500) return 0;
        const size_t row = GreedyRow(world, filter, net, obs, logits);
        const Action action = obs.legal[row];
        potions += action.kind == Action::UsePotion;
        filter.OnAction(action);
        world.Step(action);
    }
    const int32_t hp = Hp(world);
    if (hp > 0) return ro.win + (hp - ro.potion_hp * potions) / std::max(1, max_hp);
    double left = 0, full = 0;
    for (Creature enemy : CombatState(&world.sim()).Enemies()) {
        left += std::max(0, enemy.st().current_hp);
        full += std::max(1, enemy.st().max_hp);
    }
    return full > 0 ? ro.lose * (1 - std::min(1.0, left / full)) : 0;
}

// The rows of `obs` to try, most probable first (at most `top`), and for each the worth of every resampled copy
// that could be played for all of them (the same copies for every row). `env` is at the decision `obs` shows and
// is not changed; it must not be at a choice inside an effect.
struct Trial {
    std::vector<size_t> rows;
    std::vector<std::vector<double>> worth;  // [row][copy]
    size_t copies() const { return worth.empty() ? 0 : worth[0].size(); }
    double mean(size_t c) const {
        double sum = 0;
        for (double w : worth[c]) sum += w;
        return worth[c].empty() ? 0 : sum / static_cast<double>(worth[c].size());
    }
    // Mean and standard error of row c's paired difference to the policy's own row (row 0).
    void difference(size_t c, double& mean_out, double& se_out) const {
        const size_t n = copies();
        double m = 0, square = 0;
        for (size_t i = 0; i < n; ++i) m += worth[c][i] - worth[0][i];
        m /= static_cast<double>(std::max<size_t>(1, n));
        for (size_t i = 0; i < n; ++i) { const double d = worth[c][i] - worth[0][i] - m; square += d * d; }
        mean_out = m;
        se_out = n >= 2 ? std::sqrt(square / static_cast<double>(n - 1) / static_cast<double>(n)) : 0;
    }
    // The policy's row unless another one's mean is higher by more than `z` standard errors of the paired difference.
    size_t best(double z, double* gain = nullptr) const {
        size_t pick = 0;
        double top_gain = 0;
        for (size_t c = 1; c < rows.size() && copies() >= 2; ++c) {
            double m, se;
            difference(c, m, se);
            if (m > top_gain && m > z * se) { top_gain = m; pick = c; }
        }
        if (gain) *gain = top_gain;
        return pick;
    }
};

inline Trial Try(const Env& env, const ActorFilter& filter, const Observation& obs, const std::vector<float>& logits,
                 const PolicyNet& net, const Options& ro, int32_t max_hp, uint64_t seed) {
    Trial trial;
    trial.rows.resize(logits.size());
    for (size_t i = 0; i < trial.rows.size(); ++i) trial.rows[i] = i;
    std::stable_sort(trial.rows.begin(), trial.rows.end(), [&](size_t a, size_t b) { return logits[a] > logits[b]; });
    trial.rows.resize(std::min(trial.rows.size(), static_cast<size_t>(std::max(1, ro.top))));
    trial.worth.resize(trial.rows.size());
    for (int32_t n = 0; n < ro.samples; ++n) {
        std::vector<double> sample;
        try {
            for (size_t row : trial.rows) {
                std::unique_ptr<Env> world = env.Clone();
                Resample(*world, seed + static_cast<uint64_t>(n));
                sample.push_back(Worth(*world, filter, obs.legal[row], net, ro, max_hp));
            }
        } catch (const std::exception&) { continue; }  // a copy that could not be played is dropped for every row
        for (size_t c = 0; c < trial.rows.size(); ++c) trial.worth[c].push_back(sample[c]);
    }
    return trial;
}

}  // namespace sts2::nosl::rollout
