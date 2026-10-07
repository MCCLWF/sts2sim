// nosl_run: whole runs under a baseline policy, one JSON line per attempt.
//
//   nosl_run --policy random|rule0|lookahead|teacher --split dev|train|test
//            [--seed-prefix P] [--first N] [--count N] [--seed-width W]
//            [--character SILENT] [--ascension 10] [--threads N] [--max-steps N]
//            [--policy-seed N] [--time-limit-s S] [--out FILE] [--combats 0|1]
//            [--teacher-beam N] [--teacher-nodes N] [--teacher-ms N] [--teacher-cache-mib N]
//            [--record-dir DIR]
//
// --record-dir (teacher policy): every combat decision the solver's route
// answered is written as one demonstration, the actor observation plus the
// row of the teacher's action, to DIR/shard-<thread>.bin. A record is int32:
// magic 0x4E4F534C, run index, global[40], token count, action count, label
// row, tokens, actions. The labels come from a search that saw the true
// future; they are training data, never an evaluation result.
//
// Every attempt is reported: an original victory or death, a step-limit
// truncation, Unported content, or an error. A summary object follows on
// stdout. Seed prefixes are bound to the data split registered in
// docs/plans/2026-10-03-nosl-q1-foundation.md.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "engine/json.h"
#include "engine/sim/errors.h"
#include "game/All.h"
#include "nosl/observation.h"
#include "nosl/policies.h"

using namespace sts2;
using Clock = std::chrono::steady_clock;

namespace {

constexpr size_t kKinds = 10;
constexpr std::array<const char*, kKinds> kKindNames = {
    "map", "combat", "rewards", "event", "rest", "shop", "treasure", "choice", "ended", "boundary"};

struct CombatRecord {
    std::string encounter;
    int32_t act = 0, floor = 0, hp_before = 0, hp_after = 0, max_hp = 0;
    bool survived = false;
};

struct RunRecord {
    std::string seed, end, message, last_encounter;
    int32_t act = 0, floor = 0, hp = 0, max_hp = 0, gold = 0, deck = 0, relics = 0;
    int64_t steps = 0;
    std::array<int64_t, kKinds> by_kind{};
    double ms_total = 0, ms_legal = 0, ms_step = 0, ms_policy = 0;
    std::vector<CombatRecord> combats;
    nosl::PolicyStats stats;
};

double Ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }

struct Recorder {
    std::ofstream out;
    nosl::Observation obs;
    int64_t records = 0, unmatched = 0;
    void Write(const Env& env, const Action& action, int32_t run_index) {
        nosl::Observe(env, obs);
        int32_t label = -1;
        for (size_t i = 0; i < obs.legal.size(); ++i)
            if (obs.legal[i] == action) { label = static_cast<int32_t>(i); break; }
        if (label < 0) { ++unmatched; return; }
        const int32_t head[5] = {0x4E4F534C, run_index, static_cast<int32_t>(obs.token_count()),
                                 static_cast<int32_t>(obs.action_count()), label};
        auto put = [&](const int32_t* data, size_t count) {
            out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(count * sizeof(int32_t)));
        };
        put(head, 2);
        put(obs.global.data(), obs.global.size());
        put(head + 2, 3);
        put(obs.tokens.data(), obs.tokens.size());
        put(obs.actions.data(), obs.actions.size());
        ++records;
    }
};

void PlayRun(Env& env, nosl::Policy& policy, const NewRunOptions& options, uint64_t policy_seed,
             int64_t max_steps, RunRecord& r, Recorder* recorder = nullptr, int32_t run_index = 0) {
    const auto run_begin = Clock::now();
    const bool trace = std::getenv("NOSL_TRACE") != nullptr;
    policy.stats = {};
    policy.Reset(policy_seed);
    bool in_combat = false;
    auto hp = [&] { return env.state().creatures[env.state().run.player.creature].current_hp; };
    auto max_hp = [&] { return env.state().creatures[env.state().run.player.creature].max_hp; };
    auto close_combat = [&] {
        if (!in_combat) return;
        r.combats.back().hp_after = hp();
        r.combats.back().survived = hp() > 0;
        in_combat = false;
    };
    try {
        env.Reset(options);
        for (;;) {
            const GameState& g = env.state();
            if (g.combat.is_in_progress && !in_combat) {
                in_combat = true;
                r.combats.push_back({std::string(name(g.combat.encounter)), g.run.current_act_index,
                                     g.run.total_floor, hp(), 0, max_hp(), false});
            } else if (!g.combat.is_in_progress) {
                close_combat();
            }
            const DecisionKind kind = env.decision().kind;
            if (kind == DecisionKind::Ended) {
                r.end = g.run.is_victory ? "victory" : hp() <= 0 ? "death" : "ended_other";
                break;
            }
            if (r.steps >= max_steps) { r.end = "step_limit"; break; }
            const auto t0 = Clock::now();
            std::vector<Action> legal = env.LegalActions();
            const auto t1 = Clock::now();
            if (legal.empty()) throw GameError("no legal action at a " + std::string(kKindNames[static_cast<size_t>(kind)]) + " decision");
            const int32_t fallbacks = policy.stats.teacher_fallback;
            const Action action = policy.Choose(env, legal);
            const auto t2 = Clock::now();
            if (recorder && g.combat.is_in_progress && policy.stats.teacher_fallback == fallbacks)
                recorder->Write(env, action, run_index);
            if (trace)
                std::fprintf(stderr, "%s step %lld %s floor %d hp %d/%d legal %zu -> kind %d index %d target %d coord %d,%d\n",
                             r.seed.c_str(), static_cast<long long>(r.steps), kKindNames[static_cast<size_t>(kind)],
                             g.run.total_floor, hp(), max_hp(), legal.size(), static_cast<int>(action.kind),
                             action.index, action.target, action.coord.col, action.coord.row);
            env.Step(action);
            const auto t3 = Clock::now();
            r.ms_legal += Ms(t0, t1); r.ms_policy += Ms(t1, t2); r.ms_step += Ms(t2, t3);
            ++r.by_kind[static_cast<size_t>(kind)];
            ++r.steps;
        }
    } catch (const Unported& e) {
        r.end = "unported"; r.message = e.what();
    } catch (const std::exception& e) {
        r.end = "error"; r.message = e.what();
    }
    close_combat();
    const GameState& g = env.state();
    r.act = g.run.current_act_index;
    r.floor = g.run.total_floor;
    r.hp = hp(); r.max_hp = max_hp();
    r.gold = g.run.player.gold;
    r.deck = static_cast<int32_t>(g.run.player.deck.size());
    r.relics = static_cast<int32_t>(g.run.player.relics.size());
    if (!r.combats.empty()) r.last_encounter = r.combats.back().encounter;
    r.stats = policy.stats;
    r.ms_total = Ms(run_begin, Clock::now());
}

std::string ToJson(const RunRecord& r, const std::string& policy, const char* policy_class, int ascension, bool combats) {
    JsonWriter w;
    w.begin_object().field("seed", r.seed).field("policy", policy).field("policy_class", policy_class)
        .field("ascension", ascension).field("end", r.end);
    if (!r.message.empty()) w.field("message", r.message);
    w.field("act", r.act).field("floor", r.floor).field("hp", r.hp).field("max_hp", r.max_hp)
        .field("gold", r.gold).field("deck", r.deck).field("relics", r.relics)
        .field("combats", static_cast<int32_t>(r.combats.size())).field("last_encounter", r.last_encounter)
        .field("steps", r.steps).key("steps_by_kind").begin_object();
    for (size_t i = 0; i < kKinds; ++i) if (r.by_kind[i]) w.field(kKindNames[i], r.by_kind[i]);
    w.end_object().field("ms_total", r.ms_total).field("ms_legal", r.ms_legal)
        .field("ms_step", r.ms_step).field("ms_policy", r.ms_policy);
    if (!r.stats.solves.empty() || r.stats.teacher_fallback || !r.stats.teacher_errors.empty()) {
        w.key("teacher").begin_object().field("route_illegal", r.stats.route_illegal)
            .field("fallback_decisions", r.stats.teacher_fallback).key("errors").begin_object();
        for (const auto& [what, n] : r.stats.teacher_errors) w.field(what, n);
        w.end_object().key("solves").begin_array();
        for (const auto& s : r.stats.solves)
            w.begin_object().field("ms", s.ms).field("nodes", s.nodes).field("actions", s.route_actions)
                .field("won", s.won).field("stop", s.stop_reason).end_object();
        w.end_array().end_object();
    }
    if (combats) {
        w.key("combat_log").begin_array();
        for (const auto& c : r.combats)
            w.begin_object().field("encounter", c.encounter).field("act", c.act).field("floor", c.floor)
                .field("hp_before", c.hp_before).field("hp_after", c.hp_after).field("max_hp", c.max_hp)
                .field("survived", c.survived).end_object();
        w.end_array();
    }
    w.end_object();
    return w.str();
}

long PeakRssKib() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line))
        if (line.rfind("VmHWM:", 0) == 0) return std::stol(line.substr(6));
    return -1;
}

CharacterId ParseCharacter(const std::string& value) {
    for (size_t i = 1; i < kCharacterIdNames.size(); ++i)
        if (kCharacterIdNames[i] == value) return static_cast<CharacterId>(i);
    throw std::invalid_argument("unknown character " + value);
}

bool StartsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

}  // namespace

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> opt;
        for (int i = 1; i + 1 < argc; i += 2) {
            std::string flag = argv[i];
            if (!StartsWith(flag, "--")) throw std::invalid_argument("bad flag " + flag);
            opt[flag.substr(2)] = argv[i + 1];
        }
        if (argc % 2 == 0) throw std::invalid_argument("missing value for the last flag");
        auto get = [&](const char* k, const std::string& d) { return opt.count(k) ? opt[k] : d; };
        const std::string policy_name = get("policy", "");
        const std::string split = get("split", "");
        if (policy_name.empty() || split.empty()) throw std::invalid_argument("--policy and --split are required");
        const std::string prefix = get("seed-prefix", split == "dev" ? "NOSLDEV" : split == "test" ? "NOSLTEST" : "NT");
        const bool dev = StartsWith(prefix, "NOSLDEV"), test = StartsWith(prefix, "NOSLTEST");
        if ((split == "dev") != dev || (split == "test") != test || (split != "dev" && split != "test" && split != "train"))
            throw std::invalid_argument("seed prefix does not belong to split " + split);
        if (test && get("final-eval", "0") != "1")
            throw std::invalid_argument("the held-out test split needs --final-eval 1");
        const int first = std::stoi(get("first", "0")), count = std::stoi(get("count", "1"));
        const int width = std::stoi(get("seed-width", split == "train" ? "6" : "4"));
        const int ascension = std::stoi(get("ascension", "10"));
        const CharacterId character = ParseCharacter(get("character", "SILENT"));
        const int threads = std::max(1, std::stoi(get("threads", "1")));
        const int64_t max_steps = std::stoll(get("max-steps", "50000"));
        const uint64_t policy_seed = std::stoull(get("policy-seed", "1"));
        const double time_limit_s = std::stod(get("time-limit-s", "0"));
        const bool combats = get("combats", "1") != "0";
        nosl::TeacherOptions teacher;
        teacher.config.Profile.BeamWidth = std::stoi(get("teacher-beam", "60"));
        teacher.config.Profile.MaxExpandedNodes = std::stoi(get("teacher-nodes", "120000"));
        teacher.config.Profile.SoftTimeBudgetMilliseconds = std::stoi(get("teacher-ms", "120000"));
        teacher.config.TransitionCacheBytes = static_cast<size_t>(std::stoi(get("teacher-cache-mib", "128"))) * 1024 * 1024;
        std::ofstream file;
        if (opt.count("out")) {
            file.open(opt["out"]);
            if (!file) throw std::runtime_error("cannot write " + opt["out"]);
        }
        std::ostream& out = opt.count("out") ? static_cast<std::ostream&>(file) : std::cout;
        const char* policy_class = nosl::PolicyClassName(nosl::MakePolicy(policy_name, teacher)->Class());

        {   // Touch the process-wide content registries once before threads start.
            Env warm;
            warm.Reset(NewRunOptions{.seed = "NTWARMUP", .character = character, .ascension = ascension});
        }

        const std::string record_dir = get("record-dir", "");
        if (!record_dir.empty() && policy_name != "teacher")
            throw std::invalid_argument("--record-dir records teacher demonstrations: use --policy teacher");
        std::atomic<int> next{0}, thread_ids{0};
        std::atomic<int64_t> recorded{0}, unmatched{0};
        std::mutex mutex;
        std::vector<RunRecord> records;
        const auto begin = Clock::now();
        auto worker = [&] {
            std::unique_ptr<nosl::Policy> policy = nosl::MakePolicy(policy_name, teacher);
            Recorder recorder;
            if (!record_dir.empty()) {
                recorder.out.open(record_dir + "/shard-" + std::to_string(thread_ids.fetch_add(1)) + ".bin", std::ios::binary);
                if (!recorder.out) throw std::runtime_error("cannot write into " + record_dir);
            }
            struct Flush { Recorder& r; std::atomic<int64_t>& n; std::atomic<int64_t>& u; ~Flush() { n += r.records; u += r.unmatched; } } flush{recorder, recorded, unmatched};
            for (;;) {
                const int i = next.fetch_add(1);
                if (i >= count) return;
                if (time_limit_s > 0 && Ms(begin, Clock::now()) > time_limit_s * 1000) return;
                char digits[32];
                std::snprintf(digits, sizeof digits, "%0*d", width, first + i);
                RunRecord r;
                r.seed = prefix + digits;
                Env env;
                PlayRun(env, *policy, NewRunOptions{.seed = r.seed, .character = character, .ascension = ascension},
                        policy_seed * 1000003ull + static_cast<uint64_t>(first + i), max_steps, r,
                        record_dir.empty() ? nullptr : &recorder, first + i);
                std::string line = ToJson(r, policy_name, policy_class, ascension, combats);
                std::lock_guard<std::mutex> lock(mutex);
                out << line << std::endl;  // flushed: a hung run is the seed with no line
                r.combats.clear(); r.stats.solves.shrink_to_fit();
                records.push_back(std::move(r));
            }
        };
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t) pool.emplace_back(worker);
        for (auto& t : pool) t.join();
        const double wall_ms = Ms(begin, Clock::now());
        out.flush();

        std::map<std::string, int> ends, acts;
        std::vector<int32_t> floors;
        int64_t steps = 0; double ms_legal = 0, ms_step = 0, ms_policy = 0, ms_total = 0;
        std::vector<double> solve_ms; std::map<std::string, int> stops;
        int64_t solves = 0, solve_won = 0, route_illegal = 0, fallback = 0, teacher_errors = 0;
        for (const auto& r : records) {
            ++ends[r.end]; ++acts["act" + std::to_string(r.act + 1)]; floors.push_back(r.floor);
            steps += r.steps; ms_legal += r.ms_legal; ms_step += r.ms_step; ms_policy += r.ms_policy; ms_total += r.ms_total;
            for (const auto& s : r.stats.solves) { solve_ms.push_back(s.ms); ++stops[s.stop_reason]; ++solves; solve_won += s.won; }
            route_illegal += r.stats.route_illegal; fallback += r.stats.teacher_fallback;
            for (const auto& [what, n] : r.stats.teacher_errors) teacher_errors += n;
        }
        std::sort(floors.begin(), floors.end());
        std::sort(solve_ms.begin(), solve_ms.end());
        auto q = [](const auto& v, double p) { return v.empty() ? 0 : v[std::min(v.size() - 1, static_cast<size_t>(p * static_cast<double>(v.size())))]; };
        JsonWriter w;
        w.begin_object().field("summary", true).field("policy", policy_name).field("policy_class", policy_class)
            .field("split", split).field("seed_prefix", prefix).field("first", first)
            .field("attempts", static_cast<int32_t>(records.size())).field("ascension", ascension)
            .field("threads", threads).key("ends").begin_object();
        for (const auto& [k, v] : ends) w.field(k, v);
        w.end_object().key("ended_in_act").begin_object();
        for (const auto& [k, v] : acts) w.field(k, v);
        w.end_object().field("floor_min", q(floors, 0.0)).field("floor_median", q(floors, 0.5))
            .field("floor_p90", q(floors, 0.9)).field("floor_max", floors.empty() ? 0 : floors.back())
            .field("steps", steps).field("wall_s", wall_ms / 1000)
            .field("decisions_per_s", static_cast<double>(steps) / (wall_ms / 1000))
            .field("runs_per_hour", static_cast<double>(records.size()) / (wall_ms / 3600000))
            .field("thread_ms_total", ms_total).field("thread_ms_legal", ms_legal)
            .field("thread_ms_step", ms_step).field("thread_ms_policy", ms_policy)
            .field("peak_rss_mib", static_cast<double>(PeakRssKib()) / 1024);
        if (!record_dir.empty())
            w.field("demonstrations", recorded.load()).field("demonstrations_unmatched", unmatched.load());
        if (solves) {
            w.key("teacher").begin_object().field("solves", solves).field("solves_won", solve_won)
                .field("route_illegal", route_illegal).field("fallback_decisions", fallback)
                .field("solver_errors", teacher_errors)
                .field("solve_ms_median", q(solve_ms, 0.5)).field("solve_ms_p90", q(solve_ms, 0.9))
                .field("solve_ms_max", solve_ms.back()).key("stop_reasons").begin_object();
            for (const auto& [k, v] : stops) w.field(k, v);
            w.end_object().end_object();
        }
        w.end_object();
        std::cout << w.str() << std::endl;
        return ends.count("error") ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "nosl_run: " << e.what() << '\n';
        return 2;
    }
}
