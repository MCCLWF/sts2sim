// nosl_obs_check: the actor observation on real trajectories.
//
//   nosl_obs_check --policy rule0|lookahead|random --split train|dev [--count N]
//                  [--first N] [--seed-prefix P] [--ascension 10] [--max-steps N]
//
// At every decision of every run it checks
//   - structure: one action row per legal action, every token reference in range
//     and of the type its action kind needs;
//   - information boundary: a copy whose hidden piles, future room lists and
//     relic bags were reshuffled (Env::Determinize) must give identical tables;
//     after the same action on both copies, a card selection whose option
//     tokens form the same multiset must again give identical tables.
// It reports table sizes, coverage by decision kind and token type, and the
// cost of one Observe. Exit code 1 on any violation.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "engine/json.h"
#include "engine/sim/errors.h"
#include "game/All.h"
#include "nosl/observation.h"
#include "nosl/policies.h"

using namespace sts2;
using namespace sts2::nosl;
using Clock = std::chrono::steady_clock;

namespace {

constexpr std::array<const char*, 10> kKindNames = {
    "map", "combat", "rewards", "event", "rest", "shop", "treasure", "choice", "ended", "boundary"};
constexpr std::array<const char*, static_cast<size_t>(TokenType::Count)> kTokenNames = {
    "pad", "hand", "draw", "discard", "exhaust", "deck", "enemy", "power", "relic", "potion",
    "map_next", "map_node", "reward", "choice_option", "shop", "event_option", "rest_option", "treasure"};

struct Totals {
    int64_t decisions = 0, violations = 0, invariance_checked = 0, invariance_failed = 0;
    int64_t choice_pairs = 0, choice_pairs_same_content = 0, choice_pairs_failed = 0;
    int64_t unreferenced_actions = 0;
    size_t max_tokens = 0, max_actions = 0;
    std::array<int64_t, 10> by_kind{};
    std::array<size_t, 10> max_tokens_by_kind{};
    std::array<int64_t, static_cast<size_t>(TokenType::Count)> tokens_by_type{};
    double observe_ms = 0;
    std::vector<std::string> messages;
    void Fail(const std::string& what) { ++violations; if (messages.size() < 20) messages.push_back(what); }
};

TokenType TypeAt(const Observation& o, int32_t token) {
    return static_cast<TokenType>(o.tokens[static_cast<size_t>(token) * kTokenWidth + kColType]);
}

// What an action's token must be, by action kind (Pad: none required).
bool TokenFits(Action::Kind kind, TokenType type) {
    switch (kind) {
    case Action::MapMove: return type == TokenType::MapNext;
    case Action::PlayCard: return type == TokenType::Hand;
    case Action::UsePotion: case Action::DiscardPotion: return type == TokenType::Potion;
    case Action::TakeReward: return type == TokenType::Reward;
    case Action::EventOption: return type == TokenType::EventOption;
    case Action::RestOption: return type == TokenType::RestOption;
    case Action::ShopBuy: return type == TokenType::Shop;
    case Action::TreasurePick: return type == TokenType::Treasure;
    case Action::Answer: return type == TokenType::ChoiceOption;
    default: return false;
    }
}

void CheckStructure(const Env& env, const Observation& o, const std::string& where, Totals& t) {
    std::vector<Action> legal = env.LegalActions();
    if (o.legal.size() != legal.size() || o.action_count() != legal.size()) {
        t.Fail(where + ": action rows differ from the legal action count");
        return;
    }
    std::vector<Action> a = o.legal, b = legal;
    auto key = [](const Action& x) { return std::tuple(x.kind, x.index, x.target, x.coord.col, x.coord.row); };
    auto less = [&](const Action& x, const Action& y) { return key(x) < key(y); };
    std::sort(a.begin(), a.end(), less);
    std::sort(b.begin(), b.end(), less);
    if (a != b) t.Fail(where + ": the action table is not a permutation of the legal actions");
    const int32_t tokens = static_cast<int32_t>(o.token_count());
    for (size_t i = 0; i < o.action_count(); ++i) {
        const int32_t* row = &o.actions[i * kActionWidth];
        const Action::Kind kind = static_cast<Action::Kind>(row[kActKind]);
        if (kind != o.legal[i].kind) t.Fail(where + ": action row kind differs from its action");
        if (row[kActToken] >= tokens || row[kActTarget] >= tokens) { t.Fail(where + ": token reference out of range"); continue; }
        const bool needs = kind != Action::EndTurn && kind != Action::Proceed && kind != Action::Resume &&
                           !(kind == Action::Answer && o.global[kGChoiceSubset] == 2);
        if (needs && row[kActToken] < 0) { ++t.unreferenced_actions; t.Fail(where + ": action has no token"); continue; }
        if (row[kActToken] >= 0 && !TokenFits(kind, TypeAt(o, row[kActToken]))) t.Fail(where + ": action token has the wrong type");
        if (row[kActTarget] >= 0 && TypeAt(o, row[kActTarget]) != TokenType::Enemy) t.Fail(where + ": target token is not an enemy");
    }
    if (o.global[kGTokenCount] != tokens || o.global[kGActionCount] != static_cast<int32_t>(o.action_count()))
        t.Fail(where + ": counts in the global row are wrong");
}

bool Same(const Observation& x, const Observation& y) {
    return x.global == y.global && x.tokens == y.tokens && x.actions == y.actions;
}

std::vector<std::array<int32_t, kTokenWidth>> SortedTokens(const Observation& o, TokenType only) {
    std::vector<std::array<int32_t, kTokenWidth>> out;
    for (size_t i = 0; i < o.token_count(); ++i) {
        if (static_cast<TokenType>(o.tokens[i * kTokenWidth]) != only) continue;
        std::array<int32_t, kTokenWidth> t;
        std::copy_n(o.tokens.begin() + static_cast<std::ptrdiff_t>(i * kTokenWidth), kTokenWidth, t.begin());
        out.push_back(t);
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> opt;
        for (int i = 1; i + 1 < argc; i += 2) opt[std::string(argv[i]).substr(2)] = argv[i + 1];
        auto get = [&](const char* k, const std::string& d) { return opt.count(k) ? opt[k] : d; };
        const std::string policy_name = get("policy", "rule0"), split = get("split", "train");
        const std::string prefix = get("seed-prefix", split == "dev" ? "NOSLDEV" : "NTOBS");
        if (prefix.rfind("NOSLTEST", 0) == 0 || (split == "dev") != (prefix.rfind("NOSLDEV", 0) == 0))
            throw std::invalid_argument("seed prefix does not belong to split " + split);
        const int first = std::stoi(get("first", "0")), count = std::stoi(get("count", "50"));
        const int ascension = std::stoi(get("ascension", "10"));
        const int64_t max_steps = std::stoll(get("max-steps", "50000"));
        std::unique_ptr<Policy> policy = MakePolicy(policy_name, TeacherOptions{});
        Totals t;
        std::map<std::string, int> ends;
        Observation obs, shuffled, after_a, after_b;
        for (int i = 0; i < count; ++i) {
            char digits[32];
            std::snprintf(digits, sizeof digits, "%04d", first + i);
            const std::string seed = prefix + digits;
            Env env;
            std::string end = "step_limit";
            try {
                env.Reset(NewRunOptions{.seed = seed, .character = CharacterId::SILENT, .ascension = ascension});
                policy->Reset(static_cast<uint64_t>(first + i) + 1);
                for (int64_t step = 0; step < max_steps; ++step) {
                    const DecisionKind kind = env.decision().kind;
                    if (kind == DecisionKind::Ended) { end = env.state().run.is_victory ? "victory" : "death"; break; }
                    const std::string where = seed + " step " + std::to_string(step) + " " + kKindNames[static_cast<size_t>(kind)];
                    const auto begin = Clock::now();
                    Observe(env, obs);
                    t.observe_ms += std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
                    ++t.decisions;
                    ++t.by_kind[static_cast<size_t>(kind)];
                    t.max_tokens = std::max(t.max_tokens, obs.token_count());
                    t.max_actions = std::max(t.max_actions, obs.action_count());
                    t.max_tokens_by_kind[static_cast<size_t>(kind)] = std::max(t.max_tokens_by_kind[static_cast<size_t>(kind)], obs.token_count());
                    for (size_t k = 0; k < obs.token_count(); ++k) ++t.tokens_by_type[static_cast<size_t>(obs.tokens[k * kTokenWidth])];
                    CheckStructure(env, obs, where, t);

                    std::vector<Action> legal = env.LegalActions();
                    const Action action = policy->Choose(env, legal);
                    if (kind != DecisionKind::Choice && kind != DecisionKind::Boundary) {
                        // Same public state, different hidden state.
                        std::unique_ptr<Env> other = env.Clone();
                        other->Determinize(0x9E3779B97F4A7C15ull * static_cast<uint64_t>(t.decisions));
                        Observe(*other, shuffled);
                        ++t.invariance_checked;
                        if (!Same(obs, shuffled)) { ++t.invariance_failed; t.Fail(where + ": observation changed with hidden state"); }
                        // The same action on both; compare a following card selection.
                        try {
                            std::unique_ptr<Env> a = env.Clone();
                            a->Step(action);
                            other->Step(action);
                            if (a->decision().kind == DecisionKind::Choice && other->decision().kind == DecisionKind::Choice) {
                                Observe(*a, after_a);
                                Observe(*other, after_b);
                                ++t.choice_pairs;
                                if (after_a.global == after_b.global &&
                                    SortedTokens(after_a, TokenType::ChoiceOption) == SortedTokens(after_b, TokenType::ChoiceOption) &&
                                    SortedTokens(after_a, TokenType::Hand) == SortedTokens(after_b, TokenType::Hand)) {
                                    ++t.choice_pairs_same_content;
                                    if (!Same(after_a, after_b)) { ++t.choice_pairs_failed; t.Fail(where + ": card selection order follows hidden state"); }
                                }
                            }
                        } catch (const Unported&) {
                        } catch (const InvalidRequest&) {  // the action is not legal in the reshuffled copy
                        }
                    }
                    env.Step(action);
                }
            } catch (const Unported& e) {
                end = "unported";
            } catch (const std::exception& e) {
                end = "error";
                t.Fail(seed + ": " + e.what());
            }
            ++ends[end];
        }
        JsonWriter w;
        w.begin_object().field("schema_version", kObsSchemaVersion).field("policy", policy_name)
            .field("seed_prefix", prefix).field("runs", count).key("ends").begin_object();
        for (const auto& [k, v] : ends) w.field(k, v);
        w.end_object().field("decisions", t.decisions).field("violations", t.violations)
            .field("invariance_checked", t.invariance_checked).field("invariance_failed", t.invariance_failed)
            .field("choice_pairs", t.choice_pairs).field("choice_pairs_same_content", t.choice_pairs_same_content)
            .field("choice_pairs_failed", t.choice_pairs_failed)
            .field("max_tokens", static_cast<int64_t>(t.max_tokens)).field("max_actions", static_cast<int64_t>(t.max_actions))
            .field("observe_us_mean", t.decisions ? 1000 * t.observe_ms / static_cast<double>(t.decisions) : 0.0)
            .key("decisions_by_kind").begin_object();
        for (size_t k = 0; k < kKindNames.size(); ++k) if (t.by_kind[k]) w.field(kKindNames[k], t.by_kind[k]);
        w.end_object().key("max_tokens_by_kind").begin_object();
        for (size_t k = 0; k < kKindNames.size(); ++k) if (t.by_kind[k]) w.field(kKindNames[k], static_cast<int64_t>(t.max_tokens_by_kind[k]));
        w.end_object().key("tokens_by_type").begin_object();
        for (size_t k = 0; k < kTokenNames.size(); ++k) w.field(kTokenNames[k], t.tokens_by_type[k]);
        w.end_object().key("messages").begin_array();
        for (const std::string& m : t.messages) w.value(m);
        w.end_array().end_object();
        std::cout << w.str() << std::endl;
        return t.violations ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "nosl_obs_check: " << e.what() << '\n';
        return 2;
    }
}
