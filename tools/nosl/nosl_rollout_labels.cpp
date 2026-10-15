// nosl_rollout_labels: for combat decisions along recorded runs, what each
// of the policy's most probable actions is worth when the policy itself
// finishes the combat after it. Offline training data, never a NoSL result.
//
//   nosl_rollout_labels --trajectories FILE.jsonl --weights COMBAT.bin --out PREFIX [--threads N] [--limit N] [--first N]
//                       [--acts 1,2] [--kinds normal,elite,boss] [--label-every 1]
//                       [--rollout-samples 8] [--rollout-top 4] [--rollout-win 3] [--rollout-lose 1] [--rollout-potion-hp 4]
//                       [--afterstate 1]
//
// FILE holds {seed, ascension, actor_filter, action_rows} lines. Each run is
// replayed with the sampler's semantics and keeps following its recording. At
// a combat decision with more than one action (and not a choice inside an
// effect, where nothing can be resampled), the `--rollout-top` most probable
// actions of --weights are each tried on `--rollout-samples` copies whose
// hidden state is resampled (tools/nosl/rollout.h: pile orders and every
// random stream of the combat; the same copies for every action), and the
// policy, greedy, finishes the combat on each copy. `--label-every N` labels
// one decision in N, chosen by the run's seed and the decision's index.
//
// The labels are what one step of policy improvement with the policy as its
// own model of the future says (the rollout algorithm, Tesauro and Galperin
// 1996; used as a training target in expert iteration, Anthony et al. 2017).
// They carry the worth's own weights (surviving, HP, potions), not the
// product objective. A learner trained on them still reads the public
// observation only.
//
// Output: PREFIX.<thread>.bin, a stream of records, all little-endian:
//   int32 x 10: magic 'NROL', run index (line number in FILE), decision index in the run, tokens, actions, K,
//               copies, the row the recorded run took, act index, fight kind (0 weak, 1 normal, 2 elite, 3 boss)
//   int32: global [40], tokens [tokens x 16], actions [actions x 4], the K tried rows (most probable first)
//   float32: the K mean worths, then the K standard errors of the paired difference to the first row
// The observation is the actor's (behind the run's actor filter), as a sampler would have shown it.
// One JSON summary line goes to stdout.
//
// --afterstate 1 does no rollouts. For the same decisions and the same tried rows it writes what each row does
// at once, measured by taking it on a copy of the real Env: PREFIX.<thread>.feat, records of
//   int32 x 4: magic 'NFEA', run index, decision index, K;  int32: the K rows;  float32: K x 11 numbers:
//   enemy HP removed, enemy block removed, enemies killed, player block change, player HP change, energy change,
//   hand size change, combat won, player dead, round advanced, enemy poison change
// It is a diagnostic input: do the labelled advantages become predictable once the immediate effect of an action
// is given to the learner? The copy is the true hidden world (a drawn card is the real next card), so these
// numbers are an upper bound on what displayed card values could tell a policy, not an observation.
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "core/xxhash64.h"
#include "tools/nosl/rollout.h"

using namespace sts2;
namespace rollout = sts2::nosl::rollout;

namespace {

struct Config {
    std::shared_ptr<const nosl::PolicyNet> net;
    rollout::Options rollout;
    std::set<std::string> kinds;
    std::set<int32_t> acts;
    int32_t label_every = 1;
    bool afterstate = false;
};

constexpr size_t kAfterstateWidth = 11;

// What `action` does at once, on a copy of `env` (see the file comment).
void Afterstate(const Env& env, const Action& action, std::vector<float>& out) {
    struct Seen { double enemy_hp = 0, enemy_block = 0, alive = 0, block = 0, hp = 0, energy = 0, hand = 0, round = 0, poison = 0; bool combat = false; };
    auto see = [](Env& e) {
        Seen v;
        const GameState& g = e.state();
        v.combat = g.combat.is_in_progress && e.decision().kind != DecisionKind::Ended;
        v.hp = rollout::Hp(e);
        if (!g.combat.is_in_progress) return v;
        Sim* sim = &e.sim();
        Player player(sim, 1);
        PlayerCombatState pcs = player.PlayerCombatState();
        CombatState combat(sim);
        v.block = player.Creature().Block();
        if (pcs) { v.energy = pcs.Energy(); v.hand = pcs.Hand().Count(); }
        v.round = combat.RoundNumber();
        for (Creature enemy : combat.Enemies()) {
            if (!enemy.IsAlive()) continue;
            v.alive += 1;
            v.enemy_hp += enemy.CurrentHp();
            v.enemy_block += enemy.Block();
            for (PowerModel power : enemy.Powers())
                if (std::string(name(power.PowerId_())) == "POISON_POWER") v.poison += power.Amount();
        }
        return v;
    };
    std::unique_ptr<Env> copy = env.Clone();
    const Seen before = see(*copy);
    Seen after = before;
    bool failed = false;
    try { copy->Step(action); after = see(*copy); } catch (const std::exception&) { failed = true; }
    const bool dead = !failed && after.hp <= 0;
    const bool won = !failed && before.combat && !after.combat && after.hp > 0;
    const float row[kAfterstateWidth] = {
        static_cast<float>(won ? before.enemy_hp : std::max(0.0, before.enemy_hp - after.enemy_hp)),
        static_cast<float>(won ? before.enemy_block : std::max(0.0, before.enemy_block - after.enemy_block)),
        static_cast<float>(won ? before.alive : before.alive - after.alive),
        static_cast<float>(won ? 0 : after.block - before.block), static_cast<float>(after.hp - before.hp),
        static_cast<float>(won ? 0 : after.energy - before.energy), static_cast<float>(won ? 0 : after.hand - before.hand),
        won ? 1.f : 0.f, dead ? 1.f : 0.f, static_cast<float>(!won && after.round > before.round),
        static_cast<float>(won ? 0 : after.poison - before.poison)};
    out.insert(out.end(), row, row + kAfterstateWidth);
}

struct Counts { int64_t labelled = 0, changed = 0, combat_decisions = 0; std::map<int32_t, int64_t> by_act; };

void Write(std::ofstream& out, const std::vector<int32_t>& ints, const std::vector<float>& floats) {
    out.write(reinterpret_cast<const char*>(ints.data()), static_cast<std::streamsize>(ints.size() * sizeof(int32_t)));
    out.write(reinterpret_cast<const char*>(floats.data()), static_cast<std::streamsize>(floats.size() * sizeof(float)));
}

void Label(const rollout::Trajectory& t, int32_t run_index, const Config& config, std::ofstream& out, Counts& counts) {
    Env env;
    env.Reset(NewRunOptions{.seed = t.seed, .character = CharacterId::SILENT, .ascension = t.ascension});
    nosl::ActorFilter filter(t.filter);
    nosl::Observation obs;
    std::vector<float> logits;
    float value = 0;
    auto forced = [&] {
        for (;;) {
            if (env.decision().kind == DecisionKind::Ended) return;
            nosl::Observe(env, obs);
            filter.Apply(obs);
            if (obs.legal.size() != 1) return;
            filter.OnAction(obs.legal[0]);
            env.Step(obs.legal[0]);
        }
    };
    forced();
    int32_t index = 0;
    for (int32_t row : t.rows) {
        if (env.decision().kind == DecisionKind::Ended) break;
        if (row < 0 || static_cast<size_t>(row) >= obs.legal.size()) throw GameError("action row out of range");
        const GameState& g = env.state();
        if (g.combat.is_in_progress && env.decision().kind != DecisionKind::Choice) {
            ++counts.combat_decisions;
            const std::string kind = rollout::FightKind(std::string(name(g.combat.encounter)));
            const uint64_t key = xxhash64(t.seed.data(), t.seed.size(), static_cast<uint64_t>(index));
            if ((config.kinds.empty() || config.kinds.count(kind)) && (config.acts.empty() || config.acts.count(g.run.current_act_index)) &&
                key % static_cast<uint64_t>(std::max(1, config.label_every)) == 0) {
                config.net->Forward(obs, logits, value);
                if (config.afterstate) {
                    std::vector<size_t> rows(logits.size());
                    for (size_t i = 0; i < rows.size(); ++i) rows[i] = i;
                    std::stable_sort(rows.begin(), rows.end(), [&](size_t a, size_t b) { return logits[a] > logits[b]; });
                    rows.resize(std::min(rows.size(), static_cast<size_t>(std::max(1, config.rollout.top))));
                    std::vector<int32_t> ints = {0x4145464E, run_index, index, static_cast<int32_t>(rows.size())};
                    std::vector<float> floats;
                    for (size_t r : rows) { ints.push_back(static_cast<int32_t>(r)); Afterstate(env, obs.legal[r], floats); }
                    Write(out, ints, floats);
                    ++counts.labelled;
                    ++counts.by_act[g.run.current_act_index];
                    const Action taken = obs.legal[static_cast<size_t>(row)];
                    filter.OnAction(taken);
                    env.Step(taken);
                    forced();
                    ++index;
                    continue;
                }
                const int32_t max_hp = g.creatures[g.run.player.creature].max_hp;
                const rollout::Trial trial = rollout::Try(env, filter, obs, logits, *config.net, config.rollout, max_hp, key);
                if (trial.copies() >= 2) {
                    const int32_t fight = kind == "weak" ? 0 : kind == "normal" ? 1 : kind == "elite" ? 2 : 3;
                    std::vector<int32_t> ints = {0x4C4F524E, run_index, index, static_cast<int32_t>(obs.token_count()),
                                                 static_cast<int32_t>(obs.action_count()), static_cast<int32_t>(trial.rows.size()),
                                                 static_cast<int32_t>(trial.copies()), row, g.run.current_act_index, fight};
                    ints.insert(ints.end(), obs.global.begin(), obs.global.end());
                    ints.insert(ints.end(), obs.tokens.begin(), obs.tokens.end());
                    ints.insert(ints.end(), obs.actions.begin(), obs.actions.end());
                    std::vector<float> floats;
                    for (size_t r : trial.rows) ints.push_back(static_cast<int32_t>(r));
                    for (size_t c = 0; c < trial.rows.size(); ++c) floats.push_back(static_cast<float>(trial.mean(c)));
                    for (size_t c = 0; c < trial.rows.size(); ++c) {
                        double mean = 0, se = 0;
                        if (c) trial.difference(c, mean, se);
                        floats.push_back(static_cast<float>(se));
                    }
                    Write(out, ints, floats);
                    ++counts.labelled;
                    ++counts.by_act[g.run.current_act_index];
                    if (trial.best(config.rollout.z)) ++counts.changed;
                }
            }
        }
        const Action action = obs.legal[static_cast<size_t>(row)];
        filter.OnAction(action);
        env.Step(action);
        forced();
        ++index;
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> opt;
        for (int i = 1; i + 1 < argc; i += 2) opt[std::string(argv[i]).substr(2)] = argv[i + 1];
        if (!opt.count("trajectories") || !opt.count("out") || !opt.count("weights"))
            throw std::invalid_argument("--trajectories, --weights and --out are required");
        auto number = [&](const char* key, int fallback) { return opt.count(key) ? std::stoi(opt[key]) : fallback; };
        Config config;
        config.net = nosl::PolicyNet::Load(opt["weights"]);
        config.rollout.samples = number("rollout-samples", 8);
        config.rollout.top = number("rollout-top", 4);
        if (opt.count("rollout-z")) config.rollout.z = std::stod(opt["rollout-z"]);
        if (opt.count("rollout-win")) config.rollout.win = std::stod(opt["rollout-win"]);
        if (opt.count("rollout-lose")) config.rollout.lose = std::stod(opt["rollout-lose"]);
        if (opt.count("rollout-potion-hp")) config.rollout.potion_hp = std::stod(opt["rollout-potion-hp"]);
        config.label_every = number("label-every", 1);
        config.afterstate = number("afterstate", 0) != 0;
        if (opt.count("kinds")) {
            std::istringstream kinds(opt["kinds"]);
            for (std::string kind; std::getline(kinds, kind, ',');) config.kinds.insert(kind);
        }
        if (opt.count("acts")) {
            std::istringstream acts(opt["acts"]);
            for (std::string act; std::getline(acts, act, ',');) config.acts.insert(std::stoi(act));
        }
        std::vector<rollout::Trajectory> runs;
        std::ifstream in(opt["trajectories"]);
        for (std::string line; std::getline(in, line);) if (!line.empty()) runs.push_back(rollout::Parse(line));
        const size_t first = static_cast<size_t>(number("first", 0));
        const size_t last = opt.count("limit") ? std::min(runs.size(), first + static_cast<size_t>(number("limit", 0))) : runs.size();
        { Env warm; warm.Reset(NewRunOptions{.seed = "NTWARMUP", .character = CharacterId::SILENT, .ascension = 10}); }
        const auto began = std::chrono::steady_clock::now();
        std::atomic<size_t> next{first};
        std::atomic<int> failed{0};
        std::mutex mutex;
        Counts total;
        const int threads = std::max(1, number("threads", 1));
        auto worker = [&](int id) {
            std::ofstream out(opt["out"] + "." + std::to_string(id) + (config.afterstate ? ".feat" : ".bin"), std::ios::binary);
            if (!out) { ++failed; return; }
            Counts counts;
            for (;;) {
                const size_t i = next.fetch_add(1);
                if (i >= last) break;
                try { Label(runs[i], static_cast<int32_t>(i), config, out, counts); }
                catch (const std::exception& e) { ++failed; std::lock_guard<std::mutex> lock(mutex); std::cerr << runs[i].seed << ": " << e.what() << '\n'; }
                out.flush();
            }
            std::lock_guard<std::mutex> lock(mutex);
            total.labelled += counts.labelled;
            total.changed += counts.changed;
            total.combat_decisions += counts.combat_decisions;
            for (const auto& [act, n] : counts.by_act) total.by_act[act] += n;
        };
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t) pool.emplace_back(worker, t);
        for (auto& t : pool) t.join();
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
        std::cout << "{\"runs\":" << (last > first ? last - first : 0) << ",\"failed\":" << failed.load()
                  << ",\"combat_decisions\":" << total.combat_decisions << ",\"labelled\":" << total.labelled
                  << ",\"changed_at_z\":" << total.changed << ",\"by_act\":{";
        bool comma = false;
        for (const auto& [act, n] : total.by_act) { std::cout << (comma ? "," : "") << "\"" << act << "\":" << n; comma = true; }
        std::cout << "},\"threads\":" << threads << ",\"seconds\":" << seconds << "}" << std::endl;
        return failed ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "nosl_rollout_labels: " << e.what() << '\n';
        return 2;
    }
}
