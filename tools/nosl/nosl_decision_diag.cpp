// nosl_decision_diag: what a recorded policy run chose at every decision
// outside card play, with the alternatives it had.
//
//   nosl_decision_diag --trajectories FILE.jsonl --out OUT.jsonl [--weights W.bin] [--threads N] [--limit N]
//   --progress FILE.jsonl flushes every action boundary (requires --threads 1).
//
// FILE holds {seed, ascension, actor_filter, action_rows} lines (python -m
// nosl.evaluate --trajectories). Each run is replayed with the sampler's
// semantics. One line per policy decision that is not a combat decision:
// where it was (act, floor, decision kind), the public resources (hp, gold,
// deck size), the labelled actions and the one taken. With --weights (a
// nosl.export file) each line also carries the policy's probabilities and its
// value estimate, so a choice can be told apart from a coin flip.
//
// Combat decisions are summarised per combat instead (cards played, potions
// used, turns), on a line with "kind":"CombatSummary".
//
// Labels come from the actor's own observation tables, so the diagnostic
// reads nothing the actor could not see.
#include <atomic>
#include <cmath>
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
#include "nosl/policy_net.h"

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

const char* const kDecisionNames[] = {"Map", "Combat", "Rewards", "Event", "RestSite", "Shop", "Treasure", "Choice", "Ended", "Boundary"};
const char* const kActionNames[] = {"StartRun", "MapMove", "PlayCard", "EndTurn", "TakeReward", "Proceed", "EventOption",
                                    "RestOption", "ShopBuy", "TreasurePick", "UsePotion", "DiscardPotion", "Answer", "Resume"};
const char* const kRestNames[] = {"Heal", "Smith", "Mend", "Lift", "Dig", "Cook", "Clone", "Hatch", "Kindle"};
const char* const kChoiceOptionNames[] = {"Card", "Bundle", "Reward", "Alternative", "Proceed"};
const char* const kShopKindNames[] = {"CharacterCard", "ColorlessCard", "Relic", "Potion", "CardRemoval"};

template <typename T, size_t N>
std::string At(T* const (&table)[N], int32_t index) {
    return index >= 0 && static_cast<size_t>(index) < N ? std::string(table[index]) : "#" + std::to_string(index);
}

std::string CardLabel(const int32_t* t) {
    std::string s(name(static_cast<CardId>(t[nosl::kColCard])));
    for (int32_t i = 0; i < t[nosl::kColN0]; ++i) s += "+";
    return s;
}

std::string RewardLabel(const int32_t* t) {
    std::string s(name(static_cast<RewardType>(t[nosl::kColMiscA] - nosl::misc::kRewardType)));
    if (t[nosl::kColRelic]) s += ":" + std::string(name(static_cast<RelicId>(t[nosl::kColRelic])));
    if (t[nosl::kColPotion]) s += ":" + std::string(name(static_cast<PotionId>(t[nosl::kColPotion])));
    if (t[nosl::kColN1]) s += ":" + std::to_string(t[nosl::kColN1]);
    return s;
}

std::string TokenLabel(const int32_t* t) {
    using namespace nosl;
    switch (static_cast<TokenType>(t[kColType])) {
    case TokenType::Hand: return CardLabel(t);
    case TokenType::Potion: return std::string(name(static_cast<PotionId>(t[kColPotion])));
    case TokenType::MapNext: return std::string(name(static_cast<MapPointType>(t[kColMiscA] - misc::kMapPoint)));
    case TokenType::Reward: return RewardLabel(t);
    case TokenType::EventOption: {
        std::string s = std::string(name(static_cast<EventId>(t[kColMiscA] - misc::kEvent))) + "#" + std::to_string(t[kColN0]);
        if (t[kColRelic]) s += ":" + std::string(name(static_cast<RelicId>(t[kColRelic])));
        if (t[kColN3]) s += ":proceed";
        return s;
    }
    case TokenType::RestOption: return At(kRestNames, t[kColMiscA] - misc::kRestOption);
    case TokenType::Treasure: return std::string(name(static_cast<RelicId>(t[kColRelic])));
    case TokenType::Shop: {
        std::string s = At(kShopKindNames, t[kColMiscA] - misc::kShopKind);
        if (t[kColCard]) { s += ":" + std::string(name(static_cast<CardId>(t[kColCard]))); for (int32_t i = 0; i < t[kColN4]; ++i) s += "+"; }
        if (t[kColRelic]) s += ":" + std::string(name(static_cast<RelicId>(t[kColRelic])));
        if (t[kColPotion]) s += ":" + std::string(name(static_cast<PotionId>(t[kColPotion])));
        return s + "@" + std::to_string(t[kColN0]);
    }
    case TokenType::ChoiceOption: {
        const int32_t kind = t[kColMiscA] - misc::kChoiceKind;
        if (kind == 0 || kind == 1) return (kind == 1 ? "Bundle:" : "") + CardLabel(t);
        if (kind == 2) return RewardLabel(t);
        return At(kChoiceOptionNames, kind);
    }
    default: return "token" + std::to_string(t[kColType]);
    }
}

std::string ActionLabel(const nosl::Observation& obs, size_t row) {
    const int32_t* a = &obs.actions[row * nosl::kActionWidth];
    std::string s = At(kActionNames, a[nosl::kActKind]);
    if (a[nosl::kActToken] >= 0) s += ":" + TokenLabel(&obs.tokens[static_cast<size_t>(a[nosl::kActToken]) * nosl::kTokenWidth]);
    else if (a[nosl::kActKind] == Action::Answer) s += ":" + std::to_string(a[nosl::kActNumber]);
    return s;
}

void Diagnose(const Trajectory& t, const nosl::PolicyNet* net, std::vector<std::string>& lines, std::ostream* progress) {
    Env env;
    env.Reset(NewRunOptions{.seed = t.seed, .character = CharacterId::SILENT, .ascension = t.ascension});
    nosl::ActorFilter filter(t.filter);
    nosl::Observation obs;
    std::vector<float> logits;
    int32_t step = 0;
    int64_t env_step = 0;
    auto checkpoint = [&](const char* phase, bool is_forced, int32_t row) {
        if (!progress) return;
        const GameState& g = env.state();
        JsonWriter w;
        w.begin_object().field("seed", t.seed).field("phase", phase).field("policy_step", step)
            .field("env_step", env_step).field("forced", is_forced).field("row", row)
            .field("kind", At(kDecisionNames, static_cast<int32_t>(env.decision().kind)))
            .field("act", g.run.current_act_index).field("floor", g.run.total_floor)
            .field("hp", g.creatures[g.run.player.creature].current_hp)
            .field("encounter", std::string(name(g.combat.encounter)));
        if (row >= 0) w.field("action", ActionLabel(obs, static_cast<size_t>(row)));
        w.end_object();
        *progress << w.str() << '\n';
        progress->flush();  // preserve the last boundary even if Step never returns
    };
    struct Combat {
        bool open = false;
        int32_t act = 0, floor = 0, hp = 0, plays = 0, potions = 0, ends = 0, other = 0;
        int32_t forced_ends = 0, ends_with_playable = 0, energy_left = 0, incoming = 0, unblocked = 0, overblock = 0;
        std::string encounter;
    } combat;
    auto hp = [&] { return env.state().creatures[env.state().run.player.creature].current_hp; };
    auto track = [&] {
        const GameState& g = env.state();
        if (g.combat.is_in_progress && !combat.open) {
            combat = Combat{};
            combat.open = true;
            combat.act = g.run.current_act_index;
            combat.floor = g.run.total_floor;
            combat.hp = hp();
            combat.encounter = std::string(name(g.combat.encounter));
        } else if (!g.combat.is_in_progress && combat.open) {
            combat.open = false;
            JsonWriter w;
            w.begin_object().field("seed", t.seed).field("kind", "CombatSummary").field("act", combat.act).field("floor", combat.floor)
                .field("encounter", combat.encounter).field("hp_before", combat.hp).field("hp_after", hp())
                .field("plays", combat.plays).field("potions", combat.potions).field("end_turns", combat.ends)
                .field("other", combat.other).field("ends_with_playable", combat.ends_with_playable)
                .field("energy_left", combat.energy_left).field("forced_end_turns", combat.forced_ends)
                .field("incoming", combat.incoming).field("unblocked", combat.unblocked).field("overblock", combat.overblock).end_object();
            lines.push_back(w.str());
        }
    };
    // Combat accounting from what the screen showed when the action was taken.
    auto count = [&](const Action& action, bool was_forced) {
        if (action.kind == Action::PlayCard) ++combat.plays;
        else if (action.kind == Action::UsePotion) ++combat.potions;
        else if (action.kind == Action::EndTurn) {
            ++combat.ends;
            if (was_forced) ++combat.forced_ends;
            bool playable = false;
            int32_t incoming = 0;
            for (size_t i = 0; i < obs.token_count(); ++i) {
                const int32_t* tk = &obs.tokens[i * nosl::kTokenWidth];
                if (tk[nosl::kColType] == static_cast<int32_t>(nosl::TokenType::Hand) && tk[nosl::kColN4]) playable = true;
                if (tk[nosl::kColType] == static_cast<int32_t>(nosl::TokenType::Enemy) && tk[nosl::kColN7] && tk[nosl::kColN3] > 0)
                    incoming += tk[nosl::kColN3] * std::max(1, tk[nosl::kColN4]);
            }
            if (playable) { ++combat.ends_with_playable; combat.energy_left += obs.global[nosl::kGEnergy]; }
            combat.incoming += incoming;
            combat.unblocked += std::max(0, incoming - obs.global[nosl::kGBlock]);
            combat.overblock += std::max(0, obs.global[nosl::kGBlock] - incoming);
        } else ++combat.other;
    };
    auto forced = [&] {
        for (;;) {
            track();
            if (env.decision().kind == DecisionKind::Ended) return;
            nosl::Observe(env, obs);
            filter.Apply(obs);
            if (obs.legal.size() != 1) return;
            if (env.state().combat.is_in_progress) count(obs.legal[0], true);
            filter.OnAction(obs.legal[0]);
            checkpoint("before_step", true, 0);
            env.Step(obs.legal[0]);
            ++env_step;
            checkpoint("after_step", true, -1);
        }
    };
    forced();
    for (int32_t row : t.rows) {
        if (env.decision().kind == DecisionKind::Ended) break;
        if (row < 0 || static_cast<size_t>(row) >= obs.legal.size()) throw GameError("action row out of range");
        const Action action = obs.legal[static_cast<size_t>(row)];
        const GameState& g = env.state();
        const DecisionKind kind = env.decision().kind;
        const bool in_combat = g.combat.is_in_progress;
        if (in_combat) count(action, false);
        // Card play and end of turn are the bulk of a run; everything else is listed.
        if (!(in_combat && (action.kind == Action::PlayCard || action.kind == Action::EndTurn || kind == DecisionKind::Choice))) {
            JsonWriter w;
            w.begin_object().field("seed", t.seed).field("step", step).field("kind", At(kDecisionNames, static_cast<int32_t>(kind)))
                .field("in_combat", in_combat).field("act", g.run.current_act_index).field("floor", g.run.total_floor)
                .field("hp", obs.global[nosl::kGHp]).field("max_hp", obs.global[nosl::kGMaxHp]).field("gold", obs.global[nosl::kGGold])
                .field("deck", obs.global[nosl::kGDeckCount]).field("potions", obs.global[nosl::kGPotionCount])
                .field("potion_slots", obs.global[nosl::kGPotionSlots]);
            if (kind == DecisionKind::Choice) w.field("source", env.decision().request.source).field("prompt", env.decision().request.prompt);
            w.field("chosen", row).key("options").begin_array();
            for (size_t i = 0; i < obs.action_count(); ++i) w.value(ActionLabel(obs, i));
            w.end_array();
            if (net) {
                float value = 0;
                net->Forward(obs, logits, value);
                float top = logits[0];
                for (float x : logits) top = std::max(top, x);
                double sum = 0;
                for (float x : logits) sum += std::exp(static_cast<double>(x - top));
                w.key("probs").begin_array();
                for (float x : logits) w.value(std::round(std::exp(static_cast<double>(x - top)) / sum * 1000.0) / 1000.0);
                w.end_array().field("value", static_cast<double>(value));
            }
            w.end_object();
            lines.push_back(w.str());
        }
        filter.OnAction(action);
        checkpoint("before_step", false, row);
        env.Step(action);
        ++env_step;
        checkpoint("after_step", false, -1);
        ++step;
        forced();
    }
    checkpoint("finished", false, -1);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> opt;
        for (int i = 1; i + 1 < argc; i += 2) opt[std::string(argv[i]).substr(2)] = argv[i + 1];
        if (!opt.count("trajectories") || !opt.count("out")) throw std::invalid_argument("--trajectories and --out are required");
        auto number = [&](const char* key, int fallback) { return opt.count(key) ? std::stoi(opt[key]) : fallback; };
        std::ofstream progress;
        if (opt.count("progress")) {
            if (number("threads", 1) != 1) throw std::invalid_argument("--progress requires --threads 1");
            progress.open(opt["progress"]);
            if (!progress) throw std::runtime_error("cannot write " + opt["progress"]);
        }
        std::unique_ptr<nosl::PolicyNet> net;
        if (opt.count("weights")) net = nosl::PolicyNet::Load(opt["weights"]);
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
                try { Diagnose(runs[i], net.get(), lines, progress.is_open() ? &progress : nullptr); }
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
        std::cerr << "nosl_decision_diag: " << e.what() << '\n';
        return 2;
    }
}
