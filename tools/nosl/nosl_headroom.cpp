// nosl_headroom: for every combat of a recorded policy run, what an offline
// combat player gets from the same root.
//
//   nosl_headroom --trajectories FILE.jsonl --out OUT.jsonl [--threads N] [--limit N] [--first N]
//                 [--player teacher] [--teacher-beam 30] [--teacher-nodes 10000] [--teacher-ms 10000]
//                 [--player net --weights W.bin]
//                 [--player fair [--weights W.bin] [--max-turns N]]
//                 [--kinds boss,elite,normal,weak] [--max-combats-per-run N]
//
// FILE holds {seed, ascension, actor_filter, action_rows} lines (python -m
// nosl.evaluate --trajectories, or an experiment's evaluation rows). Each run
// is replayed with the sampler's semantics. At the first decision of each
// combat the Env is cloned and the offline player finishes that combat on the
// clone; the replay then goes on with the recorded policy. One line per
// combat: the root (act, floor, encounter, HP, deck, relics, potions), the
// recorded policy's result and the offline player's result.
//
// nosl_combat_diag asks a related question with one solver call per combat and
// reports the solver's projected end. This tool plays the combat out action
// by action (solving again when a route is partial or stops being legal), so
// its result is what happened in the Env, and it also records potions used,
// the deck at the root, and can compare a second public policy.
//
// The offline player is a diagnostic, never a NoSL result. `teacher` is the
// native CombatSolver route search on the true state: it sees the real draw
// order and the enemy's future rolls, so its result bounds what better play
// in this one hidden world would give; it is not what a public-information
// player can expect. `net` is another exported policy (python -m nosl.export)
// taking its most probable action behind actor filter 1, on the public
// observation only: it compares two public policies' combat play from
// identical roots. It starts each combat with a fresh actor filter.
//
// `fair` is the same solver without the foresight. Before it plans, what the
// player cannot see is resampled on a copy: the order of the draw and discard
// piles (Env::Determinize) and every random stream a combat draws from (the
// shuffle, card generation, targets, enemy moves, each monster's own). The
// plan is made on that copy and followed in the real Env only while the two
// show the same public observation; as soon as they differ (a new hand, a
// drawn card, a different enemy roll) it resamples and plans again. Its
// result is what this search gets from public information. Two limits: a
// card publicly known to be on top of the draw pile is resampled away with
// the rest; and a plan cannot be made while a choice inside an effect is
// pending (nothing can be resampled there, and the solver does not start from
// such a state), so that one answer is given by the public policy of
// --weights when there is one, else it is the first legal answer. Those
// answers are counted (`unfair` holds their number: they are not the
// solver's).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/rng.h"
#include "core/xxhash64.h"
#include "engine/json.h"
#include "engine/sim/errors.h"
#include "game/All.h"
#include "nosl/actor_filter.h"
#include "nosl/observation.h"
#include "nosl/policies.h"
#include "nosl/policy_net.h"

using namespace sts2;
using Clock = std::chrono::steady_clock;

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

std::string FightKind(const std::string& encounter) {
    auto ends = [&](const char* suffix) {
        const std::string s(suffix);
        return encounter.size() >= s.size() && encounter.compare(encounter.size() - s.size(), s.size(), s) == 0;
    };
    if (ends("_BOSS")) return "boss";
    if (ends("_ELITE")) return "elite";
    if (ends("_WEAK")) return "weak";
    return "normal";
}

struct Result {
    bool ran = false, survived = false;
    int32_t hp_after = 0, potions = 0, actions = 0, end_turns = 0, solves = 0, fallback = 0, unfair = 0;
    bool first_solve_won = false;
    double ms = 0;
    std::string error;
};

int32_t Hp(Env& env) { return env.state().creatures[env.state().run.player.creature].current_hp; }

// The offline player finishes the combat `root` is in, on a copy.
Result PlayOffline(const Env& root, const std::string& player, const nosl::TeacherOptions& options, int64_t max_steps) {
    Result r;
    r.ran = true;
    const auto begin = Clock::now();
    try {
        std::unique_ptr<Env> env = root.Clone();
        std::unique_ptr<nosl::Policy> policy = nosl::MakePolicy(player, options);
        policy->Reset(1);
        for (int64_t step = 0;; ++step) {
            if (env->decision().kind == DecisionKind::Ended || !env->state().combat.is_in_progress) break;
            if (step >= max_steps) { r.error = "step_limit"; break; }
            const std::vector<Action> legal = env->LegalActions();
            if (legal.empty()) { r.error = "no_legal_action"; break; }
            const Action action = policy->Choose(*env, legal);
            if (action.kind == Action::UsePotion) ++r.potions;
            if (action.kind == Action::EndTurn) ++r.end_turns;
            ++r.actions;
            env->Step(action);
        }
        r.hp_after = Hp(*env);
        r.survived = r.hp_after > 0 && r.error.empty();
        r.solves = static_cast<int32_t>(policy->stats.solves.size());
        r.fallback = policy->stats.teacher_fallback;
        r.first_solve_won = !policy->stats.solves.empty() && policy->stats.solves.front().won;
    } catch (const std::exception& e) {
        r.error = std::string(e.what()).substr(0, 160);
    }
    r.ms = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return r;
}

// A public policy finishes the combat `root` is in, on a copy: greedy, sampler semantics.
Result PlayNet(const Env& root, const nosl::PolicyNet& net, int64_t max_steps) {
    Result r;
    r.ran = true;
    const auto begin = Clock::now();
    try {
        std::unique_ptr<Env> env = root.Clone();
        nosl::ActorFilter filter(nosl::kActorFilterV1);
        nosl::Observation obs;
        std::vector<float> logits;
        float value = 0;
        for (int64_t step = 0;; ++step) {
            if (env->decision().kind == DecisionKind::Ended || !env->state().combat.is_in_progress) break;
            if (step >= max_steps) { r.error = "step_limit"; break; }
            nosl::Observe(*env, obs);
            filter.Apply(obs);
            if (obs.legal.empty()) { r.error = "no_legal_action"; break; }
            size_t row = 0;
            if (obs.legal.size() > 1) {
                net.Forward(obs, logits, value);
                for (size_t i = 1; i < logits.size(); ++i) if (logits[i] > logits[row]) row = i;
            }
            const Action action = obs.legal[row];
            if (action.kind == Action::UsePotion) ++r.potions;
            if (action.kind == Action::EndTurn) ++r.end_turns;
            ++r.actions;
            filter.OnAction(action);
            env->Step(action);
        }
        r.hp_after = Hp(*env);
        r.survived = r.hp_after > 0 && r.error.empty();
    } catch (const std::exception& e) {
        r.error = std::string(e.what()).substr(0, 160);
    }
    r.ms = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return r;
}

// Resample on `world` everything the player cannot see: pile orders and the combat's random streams.
void Resample(Env& world, uint64_t seed) {
    GameState& g = world.state();
    uint64_t k = seed;
    auto fresh = [&k] { k += 0x9E3779B97F4A7C15ull; return Rng(k); };
    for (RunRngType type : {RunRngType::Shuffle, RunRngType::CombatCardGeneration, RunRngType::CombatPotionGeneration,
                            RunRngType::CombatCardSelection, RunRngType::CombatEnergyCosts, RunRngType::CombatTargets,
                            RunRngType::MonsterAi, RunRngType::Niche, RunRngType::CombatOrbs})
        g.run.rng[type] = fresh();
    g.combat.encounter_rng = fresh();
    for (Creature enemy : CombatState(&world.sim()).Enemies()) enemy.st().monster.rng = fresh();
    world.Determinize(seed);  // last: it also takes the snapshot a choice inside an effect replays from
}

bool SamePublic(const Env& a, const Env& b, nosl::Observation& oa, nosl::Observation& ob) {
    nosl::Observe(a, oa);
    nosl::Observe(b, ob);
    return oa.global == ob.global && oa.tokens == ob.tokens && oa.actions == ob.actions;
}

// The solver on resampled copies finishes the combat `root` is in (see the file comment).
Result PlayFair(const Env& root, const nosl::TeacherOptions& options, const nosl::PolicyNet* net, int64_t max_steps,
                uint64_t seed) {
    Result r;
    r.ran = true;
    const auto begin = Clock::now();
    try {
        std::unique_ptr<Env> env = root.Clone();
        std::unique_ptr<Env> world;
        std::vector<Action> route;
        size_t cursor = 0;
        nosl::Observation oa, ob;
        nosl::ActorFilter filter(nosl::kActorFilterV1);
        std::vector<float> logits;
        float value = 0;
        for (int64_t step = 0;; ++step) {
            if (env->decision().kind == DecisionKind::Ended || !env->state().combat.is_in_progress) break;
            if (step >= max_steps) { r.error = "step_limit"; break; }
            const std::vector<Action> legal = env->LegalActions();
            if (legal.empty()) { r.error = "no_legal_action"; break; }
            bool plan = !world || cursor >= route.size() ||
                        std::find(legal.begin(), legal.end(), route[cursor]) == legal.end();
            if (!plan) {
                try { plan = !SamePublic(*env, *world, oa, ob); } catch (const std::exception&) { plan = true; }
            }
            if (plan && env->decision().kind == DecisionKind::Choice) {
                // No plan can be made here: one answer from the public policy, then plan again.
                ++r.unfair;
                Action answer = legal.front();
                if (net) {
                    nosl::Observe(*env, oa);
                    filter.Apply(oa);
                    size_t row = 0;
                    if (oa.legal.size() > 1) {
                        net->Forward(oa, logits, value);
                        for (size_t i = 1; i < logits.size(); ++i) if (logits[i] > logits[row]) row = i;
                    }
                    answer = oa.legal[row];
                }
                ++r.actions;
                env->Step(answer);
                world.reset();
                continue;
            }
            if (plan) {
                world = env->Clone();
                Resample(*world, seed + 0x100000001B3ull * static_cast<uint64_t>(++r.solves));
                route = combatsolver::CombatSolver::Solve(*world, options.config).Actions;
                cursor = 0;
            }
            Action action;
            if (cursor < route.size() && std::find(legal.begin(), legal.end(), route[cursor]) != legal.end()) {
                action = route[cursor++];
            } else {
                ++r.fallback;
                const auto end = std::find_if(legal.begin(), legal.end(), [](const Action& a) { return a.kind == Action::EndTurn; });
                action = end != legal.end() ? *end : legal.front();
                world.reset();
            }
            if (action.kind == Action::UsePotion) ++r.potions;
            if (action.kind == Action::EndTurn) ++r.end_turns;
            ++r.actions;
            env->Step(action);
            if (world) {
                try { world->Step(action); } catch (const std::exception&) { world.reset(); }
            }
        }
        r.hp_after = Hp(*env);
        r.survived = r.hp_after > 0 && r.error.empty();
    } catch (const std::exception& e) {
        r.error = std::string(e.what()).substr(0, 160);
    }
    r.ms = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return r;
}

struct Options {
    std::string player = "teacher";
    std::shared_ptr<const nosl::PolicyNet> net;       // --player net
    std::shared_ptr<const nosl::PolicyNet> fair_net;  // --player fair: answers choices inside effects
    nosl::TeacherOptions teacher;
    std::set<std::string> kinds;
    int32_t max_combats_per_run = 1 << 30;
};

void Replay(const Trajectory& t, const Options& options, std::vector<std::string>& lines) {
    Env env;
    env.Reset(NewRunOptions{.seed = t.seed, .character = CharacterId::SILENT, .ascension = t.ascension});
    nosl::ActorFilter filter(t.filter);
    nosl::Observation obs;
    struct Open {
        bool open = false;
        JsonWriter head;       // the root's fields, still an open object
        Result offline;
        int32_t hp_before = 0, potions = 0, end_turns = 0, actions = 0;
    } combat;
    int32_t combats_done = 0;
    auto close = [&] {
        if (!combat.open) return;
        combat.open = false;
        const bool survived = Hp(env) > 0;  // a death also ends the combat
        JsonWriter& w = combat.head;
        w.key("recorded").begin_object().field("survived", survived).field("hp_after", survived ? Hp(env) : 0)
            .field("potions", combat.potions).field("end_turns", combat.end_turns).field("actions", combat.actions).end_object();
        const Result& o = combat.offline;
        if (o.ran) {
            w.key("offline").begin_object().field("survived", o.survived).field("hp_after", o.hp_after)
                .field("potions", o.potions).field("end_turns", o.end_turns).field("actions", o.actions)
                .field("solves", o.solves).field("fallback", o.fallback).field("unfair", o.unfair)
                .field("first_solve_won", o.first_solve_won)
                .field("ms", o.ms).field("error", o.error).end_object();
        }
        w.end_object();
        lines.push_back(w.str());
    };
    auto track = [&] {
        GameState& g = env.state();
        if (g.combat.is_in_progress && !combat.open && env.decision().kind != DecisionKind::Ended) {
            combat = Open{};
            combat.open = true;
            combat.hp_before = Hp(env);
            const std::string encounter(name(g.combat.encounter));
            const std::string kind = FightKind(encounter);
            Sim* sim = &env.sim();
            Player player(sim, 1);
            JsonWriter& w = combat.head;
            w.begin_object().field("seed", t.seed).field("act", g.run.current_act_index).field("floor", g.run.total_floor)
                .field("encounter", encounter).field("fight", kind).field("hp_before", combat.hp_before)
                .field("max_hp", player.Creature().MaxHp()).field("gold", player.Gold());
            w.key("deck").begin_array();
            for (CardModel card : player.Deck().Cards()) {
                std::string label(name(card.CardId_()));
                for (int32_t i = 0; i < card.CurrentUpgradeLevel(); ++i) label += "+";
                w.value(label);
            }
            w.end_array().key("relics").begin_array();
            for (RelicModel relic : player.Relics()) w.value(std::string(name(relic.RelicId_())));
            w.end_array().key("potions").begin_array();
            for (int32_t slot = 0; slot < player.MaxPotionCount(); ++slot)
                if (PotionModel potion = player.GetPotionAtSlotIndex(slot)) w.value(std::string(name(potion.PotionId_())));
            w.end_array();
            if ((options.kinds.empty() || options.kinds.count(kind)) && combats_done < options.max_combats_per_run) {
                combat.offline = options.net ? PlayNet(env, *options.net, 4000)
                                 : options.player == "fair"
                                     ? PlayFair(env, options.teacher, options.fair_net.get(), 4000,
                                                xxhash64(t.seed.data(), t.seed.size(), static_cast<uint64_t>(combats_done)))
                                     : PlayOffline(env, options.player, options.teacher, 4000);
                ++combats_done;
            }
        } else if (!g.combat.is_in_progress && combat.open) {
            close();
        }
    };
    auto count = [&](const Action& action) {
        if (!combat.open) return;
        ++combat.actions;
        if (action.kind == Action::UsePotion) ++combat.potions;
        if (action.kind == Action::EndTurn) ++combat.end_turns;
    };
    auto forced = [&] {
        for (;;) {
            track();
            if (env.decision().kind == DecisionKind::Ended) return;
            nosl::Observe(env, obs);
            filter.Apply(obs);
            if (obs.legal.size() != 1) return;
            count(obs.legal[0]);
            filter.OnAction(obs.legal[0]);
            env.Step(obs.legal[0]);
        }
    };
    forced();
    for (int32_t row : t.rows) {
        if (env.decision().kind == DecisionKind::Ended) break;
        if (row < 0 || static_cast<size_t>(row) >= obs.legal.size()) throw GameError("action row out of range");
        const Action action = obs.legal[static_cast<size_t>(row)];
        count(action);
        filter.OnAction(action);
        env.Step(action);
        forced();
    }
    close();  // a run that ended inside a combat
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> opt;
        for (int i = 1; i + 1 < argc; i += 2) opt[std::string(argv[i]).substr(2)] = argv[i + 1];
        if (!opt.count("trajectories") || !opt.count("out")) throw std::invalid_argument("--trajectories and --out are required");
        auto number = [&](const char* key, int fallback) { return opt.count(key) ? std::stoi(opt[key]) : fallback; };
        Options options;
        if (opt.count("player")) options.player = opt["player"];
        if (options.player == "net") {
            if (!opt.count("weights")) throw std::invalid_argument("--player net needs --weights");
            options.net = nosl::PolicyNet::Load(opt["weights"]);
        }
        if (options.player == "fair" && opt.count("weights")) options.fair_net = nosl::PolicyNet::Load(opt["weights"]);
        if (opt.count("max-turns")) options.teacher.config.MaxTurns = number("max-turns", 64);
        options.teacher.config.Profile.BeamWidth = number("teacher-beam", 30);
        options.teacher.config.Profile.MaxExpandedNodes = number("teacher-nodes", 10000);
        options.teacher.config.Profile.SoftTimeBudgetMilliseconds = number("teacher-ms", 10000);
        options.teacher.config.TransitionCacheBytes = static_cast<size_t>(number("teacher-cache-mib", 128)) * 1024 * 1024;
        options.max_combats_per_run = number("max-combats-per-run", 1 << 30);
        if (opt.count("kinds")) {
            std::istringstream kinds(opt["kinds"]);
            for (std::string kind; std::getline(kinds, kind, ',');) options.kinds.insert(kind);
        }
        std::vector<Trajectory> runs;
        std::ifstream in(opt["trajectories"]);
        for (std::string line; std::getline(in, line);) if (!line.empty()) runs.push_back(Parse(line));
        const size_t first = static_cast<size_t>(number("first", 0));
        if (first) runs.erase(runs.begin(), runs.begin() + static_cast<std::ptrdiff_t>(std::min(first, runs.size())));
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
                try { Replay(runs[i], options, lines); }
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
        std::cerr << "nosl_headroom: " << e.what() << '\n';
        return 2;
    }
}
