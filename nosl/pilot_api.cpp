// C ABI of nosl::Pilot for the game-side executor (tools/NoslPilot).
//
//   void* nosl_pilot_create(seed, ascension, actor_filter, weights_path, policy_seed, greedy)
//   const char* nosl_pilot_next(pilot)    the next action as JSON (nosl/pilot.h), or null on failure
//   const char* nosl_pilot_peek(pilot)    the pending decision as JSON, nothing advanced, or null on failure
//   const char* nosl_pilot_last_error()   why create or next failed
//   void nosl_pilot_destroy(pilot)
//
//   int32_t nosl_pilot_observe(pilot, global, tokens, max_tokens, actions, legal, max_actions, counts)
//       the shadow's observation at the pending decision, before the actor filter: the three tables and,
//       per action row, the Env action as {kind, index, target, col, row}; counts = {tokens, actions}.
//       Returns 0, -1 on failure, -2 when a buffer is too small (counts still filled).
//
// The actor on tables alone (nosl/actor.h), for an encoder that reads the game itself:
//   void* nosl_actor_create(weights_path, actor_filter, policy_seed, greedy)
//   int32_t nosl_actor_decide(actor, global, tokens, n_tokens, actions, legal, n_actions, out_prob, out_value, out_rows)
//       the index of the chosen action in the caller's own (unfiltered) table, or -1 on failure;
//       out_rows is how many rows the filter left (1: taken without the network).
//   void nosl_actor_destroy(actor)
//
//   int32_t nosl_id(table, name)   a content id by its entry name; table is one of card, relic, potion, monster,
//       power, encounter, event, enchantment, affliction, character. -1 when unknown.
//
// Strings are owned by the library and stay valid until the next call on the
// same thread. The character is Silent, the one this work trains.
#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>

#include "content/generated/ids.h"
#include "engine/sim/errors.h"
#include "nosl/actor.h"
#include "nosl/pilot.h"

namespace {

thread_local std::string g_error;
thread_local std::string g_line;

}  // namespace

extern "C" {

void* nosl_pilot_create(const char* seed, int32_t ascension, int32_t actor_filter, const char* weights,
                        uint64_t policy_seed, int32_t greedy) {
    try {
        if (!seed || !weights) throw std::invalid_argument("seed and weights are required");
        sts2::nosl::PilotOptions options;
        options.seed = seed;
        options.ascension = ascension;
        options.actor_filter = actor_filter;
        options.weights = weights;
        options.policy_seed = policy_seed;
        options.greedy = greedy != 0;
        return new sts2::nosl::Pilot(options);
    } catch (const std::exception& e) {
        g_error = e.what();
        return nullptr;
    }
}

const char* nosl_pilot_next(void* pilot) {
    try {
        if (!pilot) throw std::invalid_argument("null pilot");
        g_line = static_cast<sts2::nosl::Pilot*>(pilot)->Next();
        return g_line.c_str();
    } catch (const sts2::Unported& e) {
        g_error = std::string("unported: ") + e.what();
    } catch (const std::exception& e) {
        g_error = e.what();
    }
    return nullptr;
}

const char* nosl_pilot_peek(void* pilot) {
    try {
        if (!pilot) throw std::invalid_argument("null pilot");
        g_line = static_cast<const sts2::nosl::Pilot*>(pilot)->Peek();
        return g_line.c_str();
    } catch (const std::exception& e) {
        g_error = e.what();
    }
    return nullptr;
}

int32_t nosl_pilot_observe(void* pilot, int32_t* global, int32_t* tokens, int32_t max_tokens, int32_t* actions, int32_t* legal,
                           int32_t max_actions, int32_t* counts) {
    try {
        if (!pilot || !global || !tokens || !actions || !legal || !counts) throw std::invalid_argument("null argument");
        sts2::nosl::Observation obs;
        static_cast<const sts2::nosl::Pilot*>(pilot)->Observe(obs);
        counts[0] = static_cast<int32_t>(obs.token_count());
        counts[1] = static_cast<int32_t>(obs.action_count());
        if (counts[0] > max_tokens || counts[1] > max_actions) return -2;
        std::memcpy(global, obs.global.data(), obs.global.size() * sizeof(int32_t));
        std::memcpy(tokens, obs.tokens.data(), obs.tokens.size() * sizeof(int32_t));
        std::memcpy(actions, obs.actions.data(), obs.actions.size() * sizeof(int32_t));
        for (size_t i = 0; i < obs.legal.size(); ++i) {
            const sts2::Action& a = obs.legal[i];
            const int32_t row[5] = {static_cast<int32_t>(a.kind), a.index, a.target, a.coord.col, a.coord.row};
            std::memcpy(legal + i * 5, row, sizeof(row));
        }
        return 0;
    } catch (const std::exception& e) {
        g_error = e.what();
        return -1;
    }
}

void* nosl_actor_create(const char* weights, int32_t actor_filter, uint64_t policy_seed, int32_t greedy) {
    try {
        if (!weights) throw std::invalid_argument("weights are required");
        return new sts2::nosl::Actor(weights, actor_filter, policy_seed, greedy != 0);
    } catch (const std::exception& e) {
        g_error = e.what();
        return nullptr;
    }
}

int32_t nosl_actor_decide(void* actor, const int32_t* global, const int32_t* tokens, int32_t n_tokens, const int32_t* actions,
                          const int32_t* legal, int32_t n_actions, double* out_prob, float* out_value, int32_t* out_rows) {
    try {
        if (!actor || !global || !tokens || !actions || !legal || n_tokens < 0 || n_actions <= 0)
            throw std::invalid_argument("bad argument");
        sts2::nosl::Observation obs;
        obs.global.assign(global, global + sts2::nosl::kGlobalWidth);
        obs.tokens.assign(tokens, tokens + static_cast<size_t>(n_tokens) * sts2::nosl::kTokenWidth);
        obs.actions.assign(actions, actions + static_cast<size_t>(n_actions) * sts2::nosl::kActionWidth);
        for (int32_t i = 0; i < n_actions; ++i) {
            const int32_t* row = legal + static_cast<size_t>(i) * 5;
            obs.legal.push_back(sts2::Action{.kind = static_cast<sts2::Action::Kind>(row[0]),
                                             .coord = sts2::MapCoord{static_cast<int8_t>(row[3]), static_cast<int8_t>(row[4])},
                                             .index = row[1], .target = row[2]});
        }
        const std::vector<sts2::Action> offered = obs.legal;
        const sts2::nosl::Actor::Decision d = static_cast<sts2::nosl::Actor*>(actor)->Decide(obs);
        if (out_prob) *out_prob = d.prob;
        if (out_value) *out_value = d.value;
        if (out_rows) *out_rows = static_cast<int32_t>(d.rows);
        for (size_t i = 0; i < offered.size(); ++i)
            if (offered[i] == d.action) return static_cast<int32_t>(i);
        throw sts2::GameError("actor: the chosen action is not in the caller's table");
    } catch (const std::exception& e) {
        g_error = e.what();
        return -1;
    }
}

void nosl_actor_destroy(void* actor) { delete static_cast<sts2::nosl::Actor*>(actor); }

int32_t nosl_id(const char* table, const char* name) {
    if (!table || !name) return -1;
    const std::string_view t(table), n(name);
    auto find = [&](const auto& names) -> int32_t {
        for (size_t i = 0; i < names.size(); ++i) if (names[i] == n) return static_cast<int32_t>(i);
        return -1;
    };
    if (t == "card") return find(sts2::kCardIdNames);
    if (t == "relic") return find(sts2::kRelicIdNames);
    if (t == "potion") return find(sts2::kPotionIdNames);
    if (t == "monster") return find(sts2::kMonsterIdNames);
    if (t == "power") return find(sts2::kPowerIdNames);
    if (t == "encounter") return find(sts2::kEncounterIdNames);
    if (t == "event") return find(sts2::kEventIdNames);
    if (t == "enchantment") return find(sts2::kEnchantmentIdNames);
    if (t == "affliction") return find(sts2::kAfflictionIdNames);
    if (t == "character") return find(sts2::kCharacterIdNames);
    return -1;
}

const char* nosl_pilot_last_error() { return g_error.c_str(); }

void nosl_pilot_destroy(void* pilot) { delete static_cast<sts2::nosl::Pilot*>(pilot); }

}
