// nosl_actor_check: does the deployed actor (nosl/actor.h) decide what a recorded greedy run decided?
//
//   nosl_actor_check --trajectories FILE.jsonl --weights W.bin|P.bundle [--limit N] [--threads N]
//
// FILE holds {seed, ascension, actor_filter, action_rows} lines of greedy runs
// (nosl.strategy_q evaluate, nosl.evaluate --greedy). Each run is replayed with
// the sampler's semantics. At every decision with more than one row the actor
// is asked, on the same observation tables, for its greedy decision, and that
// is compared with the recorded row; the replay then follows the record, so
// one difference does not hide the decisions after it. Differences are
// expected only where two actions are nearly tied: the record was computed
// by PyTorch on a GPU, the actor is the native float32 forward pass.
//
// Prints one JSON object: runs, decisions, same, different (split inside and
// outside combat, and how many outside-combat decisions the strategy network
// replaced), and the first differences.
#include <atomic>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "engine/json.h"
#include "engine/sim/errors.h"
#include "game/All.h"
#include "nosl/actor.h"
#include "nosl/observation.h"

using namespace sts2;

namespace {

struct Trajectory { std::string seed; int ascension = 10, filter = 0; std::vector<int32_t> rows; };

bool Field(const std::string& line, const std::string& key, std::string& out) {
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

Trajectory Parse(const std::string& line) {
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

struct Tally {
    int64_t decisions = 0, same = 0, different_combat = 0, different_outside = 0, replaced = 0;
    std::vector<std::string> examples;
};

void Check(const Trajectory& t, const std::string& weights, Tally& tally) {
    Env env;
    env.Reset(NewRunOptions{.seed = t.seed, .character = CharacterId::SILENT, .ascension = t.ascension});
    nosl::Actor actor(weights, t.filter, 1, true);
    nosl::Observation obs;
    int32_t step = 0;
    for (;;) {
        if (env.decision().kind == DecisionKind::Ended) break;
        nosl::Observe(env, obs);
        const bool outside = obs.global[nosl::kGInCombat] == 0;
        const nosl::Actor::Decision d = actor.Decide(obs);  // prunes obs with the filter
        if (d.forced) { env.Step(d.action); continue; }
        if (static_cast<size_t>(step) >= t.rows.size()) throw GameError("the run outlived its record");
        const int32_t row = t.rows[static_cast<size_t>(step)];
        if (row < 0 || static_cast<size_t>(row) >= obs.legal.size()) throw GameError("action row out of range");
        ++tally.decisions;
        if (d.replaced) ++tally.replaced;
        if (static_cast<size_t>(row) == d.row) ++tally.same;
        else {
            ++(outside ? tally.different_outside : tally.different_combat);
            if (tally.examples.size() < 5)
                tally.examples.push_back(t.seed + " step " + std::to_string(step) + (outside ? " outside" : " combat") +
                                         ": recorded row " + std::to_string(row) + ", actor row " + std::to_string(d.row));
        }
        const Action action = obs.legal[static_cast<size_t>(row)];
        actor.Took(action);
        env.Step(action);
        ++step;
    }
    if (static_cast<size_t>(step) != t.rows.size()) throw GameError("the record outlived its run");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> opt;
        for (int i = 1; i + 1 < argc; i += 2) opt[std::string(argv[i]).substr(2)] = argv[i + 1];
        if (!opt.count("trajectories") || !opt.count("weights")) throw std::invalid_argument("--trajectories and --weights are required");
        auto number = [&](const char* key, int fallback) { return opt.count(key) ? std::stoi(opt[key]) : fallback; };
        std::vector<Trajectory> runs;
        std::ifstream in(opt["trajectories"]);
        for (std::string line; std::getline(in, line);) if (!line.empty()) runs.push_back(Parse(line));
        if (opt.count("limit") && runs.size() > static_cast<size_t>(number("limit", 0))) runs.resize(static_cast<size_t>(number("limit", 0)));
        std::atomic<size_t> next{0};
        std::atomic<int> failed{0};
        std::mutex mutex;
        Tally total;
        auto worker = [&] {
            for (;;) {
                const size_t i = next.fetch_add(1);
                if (i >= runs.size()) return;
                Tally tally;
                try { Check(runs[i], opt["weights"], tally); }
                catch (const std::exception& e) { ++failed; std::lock_guard<std::mutex> lock(mutex); std::cerr << runs[i].seed << ": " << e.what() << '\n'; continue; }
                std::lock_guard<std::mutex> lock(mutex);
                total.decisions += tally.decisions;
                total.same += tally.same;
                total.different_combat += tally.different_combat;
                total.different_outside += tally.different_outside;
                total.replaced += tally.replaced;
                for (const std::string& e : tally.examples) if (total.examples.size() < 5) total.examples.push_back(e);
            }
        };
        std::vector<std::thread> pool;
        for (int t = 0; t < std::max(1, number("threads", 1)); ++t) pool.emplace_back(worker);
        for (auto& t : pool) t.join();
        JsonWriter w;
        w.begin_object().field("runs", static_cast<int64_t>(runs.size())).field("failed", failed.load())
            .field("decisions", total.decisions).field("same", total.same)
            .field("different_combat", total.different_combat).field("different_outside", total.different_outside)
            .field("replaced_outside", total.replaced).key("examples").begin_array();
        for (const std::string& e : total.examples) w.value(e);
        w.end_array().end_object();
        std::cout << w.str() << std::endl;
        return failed ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "nosl_actor_check: " << e.what() << '\n';
        return 2;
    }
}
