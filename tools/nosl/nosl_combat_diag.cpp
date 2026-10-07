// nosl_combat_diag: at every combat a recorded policy run entered, what the
// policy did and what the privileged solver would have done from that root.
//
//   nosl_combat_diag --trajectories FILE.jsonl --out OUT.jsonl [--threads N] [--limit N]
//                    [--teacher-beam 8] [--teacher-nodes 3000] [--teacher-ms 5000]
//
// FILE holds {seed, ascension, actor_filter, action_rows} lines (python -m
// nosl.evaluate --trajectories). Each run is replayed with the sampler's
// semantics. At the first decision of each combat the solver is run on a copy
// of that state; the replay then continues with the policy's own actions.
// One line per combat: floor, act, encounter, hp_before, max_hp, the policy's
// outcome (survived, hp_after) and the solver's projection (won, final_hp).
//
// The solver sees the true future; its line is a diagnostic reference for
// combat play from the same deck, not a result the policy is held to.
#include <atomic>
#include <chrono>
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
#include "nosl/actor_filter.h"
#include "nosl/observation.h"
#include "search/combatsolver/solver.h"

using namespace sts2;

namespace {

struct Trajectory { std::string seed; int ascension = 10, filter = 0; std::vector<int32_t> rows; };

// Just enough JSON for the trajectory lines this project writes.
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

void Diagnose(const Trajectory& t, const combatsolver::CombatSearchConfig& config, std::vector<std::string>& lines) {
    Env env;
    env.Reset(NewRunOptions{.seed = t.seed, .character = CharacterId::SILENT, .ascension = t.ascension});
    nosl::ActorFilter filter(t.filter);
    nosl::Observation obs;
    bool in_combat = false;
    JsonWriter open;  // the combat being fought, completed when it ends
    auto hp = [&] { return env.state().creatures[env.state().run.player.creature].current_hp; };
    auto track = [&] {
        const GameState& g = env.state();
        if (g.combat.is_in_progress && !in_combat) {
            in_combat = true;
            open = JsonWriter();
            open.begin_object().field("seed", t.seed).field("act", g.run.current_act_index).field("floor", g.run.total_floor)
                .field("encounter", name(g.combat.encounter)).field("hp_before", hp())
                .field("max_hp", g.creatures[g.run.player.creature].max_hp);
            try {
                const auto begin = std::chrono::steady_clock::now();
                const combatsolver::CombatSearchResult result = combatsolver::CombatSolver::Solve(env, config);
                open.field("teacher_won", result.Quality.Won).field("teacher_final_hp", result.FinalPlayerHp)
                    .field("teacher_stop", result.Metrics.StopReason)
                    .field("teacher_ms", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
            } catch (const std::exception& e) {
                open.field("teacher_error", std::string(e.what()).substr(0, 120));
            }
        } else if (!g.combat.is_in_progress && in_combat) {
            in_combat = false;
            open.field("policy_survived", hp() > 0).field("policy_hp_after", hp()).end_object();
            lines.push_back(open.str());
        }
    };
    auto forced = [&] {
        for (;;) {
            track();
            if (env.decision().kind == DecisionKind::Ended) return;
            nosl::Observe(env, obs);
            filter.Apply(obs);
            if (obs.legal.size() != 1) return;
            filter.OnAction(obs.legal[0]);
            env.Step(obs.legal[0]);
        }
    };
    forced();
    for (int32_t row : t.rows) {
        if (env.decision().kind == DecisionKind::Ended) break;
        if (row < 0 || static_cast<size_t>(row) >= obs.legal.size()) throw GameError("action row out of range");
        const Action action = obs.legal[static_cast<size_t>(row)];
        filter.OnAction(action);
        env.Step(action);
        forced();
    }
    if (in_combat) {  // the run was cut inside a combat (step limit)
        open.field("policy_survived", hp() > 0).field("policy_hp_after", hp()).field("unfinished", true).end_object();
        lines.push_back(open.str());
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> opt;
        for (int i = 1; i + 1 < argc; i += 2) opt[std::string(argv[i]).substr(2)] = argv[i + 1];
        if (!opt.count("trajectories") || !opt.count("out")) throw std::invalid_argument("--trajectories and --out are required");
        auto number = [&](const char* key, int fallback) { return opt.count(key) ? std::stoi(opt[key]) : fallback; };
        combatsolver::CombatSearchConfig config;
        config.Profile.BeamWidth = number("teacher-beam", 8);
        config.Profile.MaxExpandedNodes = number("teacher-nodes", 3000);
        config.Profile.SoftTimeBudgetMilliseconds = number("teacher-ms", 5000);
        config.TransitionCacheBytes = 128ull * 1024 * 1024;
        std::vector<Trajectory> runs;
        std::ifstream in(opt["trajectories"]);
        for (std::string line; std::getline(in, line);) if (!line.empty()) runs.push_back(Parse(line));
        if (opt.count("limit") && runs.size() > static_cast<size_t>(number("limit", 0))) runs.resize(static_cast<size_t>(number("limit", 0)));
        std::ofstream out(opt["out"]);
        if (!out) throw std::runtime_error("cannot write " + opt["out"]);
        { Env warm; warm.Reset(NewRunOptions{.seed = "NTWARMUP", .character = CharacterId::SILENT, .ascension = 10}); }
        std::atomic<size_t> next{0};
        std::atomic<int> failed{0};
        std::mutex mutex;
        auto worker = [&] {
            for (;;) {
                const size_t i = next.fetch_add(1);
                if (i >= runs.size()) return;
                std::vector<std::string> lines;
                try { Diagnose(runs[i], config, lines); }
                catch (const std::exception& e) { ++failed; std::lock_guard<std::mutex> lock(mutex); std::cerr << runs[i].seed << ": " << e.what() << '\n'; }
                std::lock_guard<std::mutex> lock(mutex);
                for (const std::string& line : lines) out << line << '\n';
                out.flush();
            }
        };
        std::vector<std::thread> pool;
        for (int t = 0; t < std::max(1, number("threads", 1)); ++t) pool.emplace_back(worker);
        for (auto& t : pool) t.join();
        std::cout << "{\"runs\":" << runs.size() << ",\"failed\":" << failed.load() << "}" << std::endl;
        return failed ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "nosl_combat_diag: " << e.what() << '\n';
        return 2;
    }
}
