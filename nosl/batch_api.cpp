// See batch_api.h.
#include "nosl/batch_api.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "engine/sim/errors.h"
#include "game/All.h"
#include "nosl/actor_filter.h"
#include "nosl/observation.h"
#include "nosl/policy_net.h"
#include "nosl/resample.h"

using namespace sts2;

namespace {

struct Slot {
    std::unique_ptr<Env> env;
    nosl::Observation obs;
    nosl::ActorFilter filter;
    int32_t status = NOSL_ERROR;  // no run yet
    int32_t steps = 0;
    int32_t last_encounter = 0;
    std::string error = "no run: reset this slot first";
};

struct Batch {
    std::vector<Slot> slots;
    int32_t max_tokens = 0, max_actions = 0, max_steps = 0, filter = nosl::kActorFilterNone;
    size_t active = 0;
};

// Run the slot forward to a decision that needs the policy, then observe it.
// The observation is the actor's: pruned by the batch's actor filter, and a
// decision left with one action is taken without asking (auto_forced).
void Settle(Batch& batch, Slot& slot, bool auto_forced) {
    try {
        for (;;) {
            const GameState& g = slot.env->state();
            if (g.combat.is_in_progress) slot.last_encounter = static_cast<int32_t>(g.combat.encounter);
            const bool ended = slot.env->decision().kind == DecisionKind::Ended;
            if (ended) slot.status = g.run.is_victory ? NOSL_VICTORY : NOSL_DEATH;
            else if (slot.steps >= batch.max_steps) slot.status = NOSL_STEP_LIMIT;
            nosl::Observe(*slot.env, slot.obs);
            if (slot.status != NOSL_RUNNING) break;
            slot.filter.Apply(slot.obs);
            if (!auto_forced || slot.obs.legal.size() != 1) break;
            slot.filter.OnAction(slot.obs.legal[0]);
            slot.env->Step(slot.obs.legal[0]);
            ++slot.steps;
        }
        if (slot.status == NOSL_RUNNING) {
            if (slot.obs.legal.empty()) throw GameError("a decision with no legal action");
            if (slot.obs.token_count() > static_cast<size_t>(batch.max_tokens) ||
                slot.obs.action_count() > static_cast<size_t>(batch.max_actions)) {
                slot.status = NOSL_CAPACITY;
                slot.error = "observation needs " + std::to_string(slot.obs.token_count()) + " tokens and " +
                             std::to_string(slot.obs.action_count()) + " actions";
            }
        }
    } catch (const Unported& e) {
        slot.status = NOSL_UNPORTED;
        slot.error = e.what();
    } catch (const std::exception& e) {
        slot.status = NOSL_ERROR;
        slot.error = e.what();
    }
}

Batch* Cast(void* handle) { return static_cast<Batch*>(handle); }

thread_local std::string g_policy_error;

}  // namespace

extern "C" {

void nosl_obs_shape(int32_t out[4]) {
    out[0] = nosl::kObsSchemaVersion;
    out[1] = static_cast<int32_t>(nosl::kGlobalWidth);
    out[2] = static_cast<int32_t>(nosl::kTokenWidth);
    out[3] = static_cast<int32_t>(nosl::kActionWidth);
}

void* nosl_batch_create(int32_t runs, int32_t max_tokens, int32_t max_actions, int32_t max_steps) {
    if (runs <= 0 || max_tokens <= 0 || max_actions <= 0 || max_steps <= 0) return nullptr;
    auto batch = std::make_unique<Batch>();
    batch->slots.resize(static_cast<size_t>(runs));
    batch->max_tokens = max_tokens;
    batch->max_actions = max_actions;
    batch->max_steps = max_steps;
    batch->active = batch->slots.size();
    return batch.release();
}

void nosl_batch_destroy(void* handle) { delete Cast(handle); }

int32_t nosl_batch_reset(void* handle, int32_t index, const char* seed, int32_t character, int32_t ascension) {
    Batch* batch = Cast(handle);
    if (!batch || index < 0 || static_cast<size_t>(index) >= batch->slots.size() || !seed) return -1;
    Slot& slot = batch->slots[static_cast<size_t>(index)];
    slot.status = NOSL_RUNNING;
    slot.steps = 0;
    slot.last_encounter = 0;
    slot.error.clear();
    slot.filter = nosl::ActorFilter(batch->filter);
    try {
        slot.env = std::make_unique<Env>();
        slot.env->Reset(NewRunOptions{.seed = seed, .character = static_cast<CharacterId>(character), .ascension = ascension});
    } catch (const Unported& e) {
        slot.status = NOSL_UNPORTED;
        slot.error = e.what();
        return slot.status;
    } catch (const std::exception& e) {
        slot.status = NOSL_ERROR;
        slot.error = e.what();
        return slot.status;
    }
    Settle(*batch, slot, true);
    return slot.status;
}

int32_t nosl_batch_set_active(void* handle, int32_t active) {
    Batch* batch = Cast(handle);
    if (!batch || active < 0 || static_cast<size_t>(active) > batch->slots.size()) return -1;
    batch->active = static_cast<size_t>(active);
    return 0;
}

int32_t nosl_batch_set_filter(void* handle, int32_t version) {
    Batch* batch = Cast(handle);
    if (!batch || (version != nosl::kActorFilterNone && version != nosl::kActorFilterV1)) return -1;
    batch->filter = version;
    return 0;
}

int32_t nosl_batch_swap(void* handle, int32_t a, int32_t b) {
    Batch* batch = Cast(handle);
    if (!batch || a < 0 || b < 0 || static_cast<size_t>(a) >= batch->slots.size() ||
        static_cast<size_t>(b) >= batch->slots.size()) return -1;
    std::swap(batch->slots[static_cast<size_t>(a)], batch->slots[static_cast<size_t>(b)]);
    return 0;
}

int32_t nosl_batch_fork(void* handle, int32_t source, int32_t destination) {
    Batch* batch = Cast(handle);
    if (!batch || source < 0 || destination < 0 || source == destination ||
        static_cast<size_t>(source) >= batch->slots.size() ||
        static_cast<size_t>(destination) >= batch->slots.size()) return -1;
    const Slot& from = batch->slots[static_cast<size_t>(source)];
    if (!from.env || from.status != NOSL_RUNNING) return -1;
    Slot& to = batch->slots[static_cast<size_t>(destination)];
    try {
        Slot copy;
        copy.env = from.env->Clone();
        copy.obs = from.obs;
        copy.filter = from.filter;
        copy.status = from.status;
        copy.steps = from.steps;
        copy.last_encounter = from.last_encounter;
        copy.error = from.error;
        to = std::move(copy);
    } catch (const std::exception& error) {
        to.status = NOSL_ERROR;
        to.error = std::string("offline slot fork: ") + error.what();
    }
    return to.status;
}

int32_t nosl_batch_resample(void* handle, int32_t index, uint64_t seed) {
    Batch* batch = Cast(handle);
    if (!batch || index < 0 || static_cast<size_t>(index) >= batch->slots.size()) return -1;
    Slot& slot = batch->slots[static_cast<size_t>(index)];
    if (!slot.env || slot.status != NOSL_RUNNING) return -1;
    if (!slot.env->state().combat.is_in_progress || slot.env->decision().kind != DecisionKind::Combat) return -2;
    try {
        nosl::ResampleHidden(*slot.env, seed);
    } catch (const std::exception& error) {
        slot.status = NOSL_ERROR;
        slot.error = std::string("offline slot resample: ") + error.what();
    }
    return slot.status;
}

int32_t nosl_batch_step(void* handle, const int32_t* choice, int32_t auto_forced,
                        int32_t* global, int32_t* tokens, int32_t* actions,
                        int32_t* counts, int32_t* status, int32_t* steps) {
    Batch* batch = Cast(handle);
    if (!batch || !choice || !global || !tokens || !actions || !counts || !status || !steps) return -1;
    const size_t token_row = static_cast<size_t>(batch->max_tokens) * nosl::kTokenWidth;
    const size_t action_row = static_cast<size_t>(batch->max_actions) * nosl::kActionWidth;
    for (size_t i = 0; i < batch->active; ++i) {
        Slot& slot = batch->slots[i];
        if (choice[i] >= 0 && slot.status == NOSL_RUNNING) {
            if (static_cast<size_t>(choice[i]) >= slot.obs.legal.size()) {
                slot.status = NOSL_ERROR;
                slot.error = "action row out of range";
            } else {
                try {
                    slot.filter.OnAction(slot.obs.legal[static_cast<size_t>(choice[i])]);
                    slot.env->Step(slot.obs.legal[static_cast<size_t>(choice[i])]);
                    ++slot.steps;
                    Settle(*batch, slot, auto_forced != 0);
                } catch (const Unported& e) {
                    slot.status = NOSL_UNPORTED;
                    slot.error = e.what();
                } catch (const std::exception& e) {
                    slot.status = NOSL_ERROR;
                    slot.error = e.what();
                }
            }
        }
        int32_t* g = global + i * nosl::kGlobalWidth;
        int32_t* t = tokens + i * token_row;
        int32_t* a = actions + i * action_row;
        std::memset(g, 0, nosl::kGlobalWidth * sizeof(int32_t));
        std::memset(t, 0, token_row * sizeof(int32_t));
        std::memset(a, 0, action_row * sizeof(int32_t));
        counts[2 * i] = counts[2 * i + 1] = 0;
        const bool fits = slot.obs.tokens.size() <= token_row && slot.obs.actions.size() <= action_row;
        if (slot.obs.global.size() == nosl::kGlobalWidth && fits &&
            (slot.status == NOSL_RUNNING || slot.status == NOSL_VICTORY || slot.status == NOSL_DEATH || slot.status == NOSL_STEP_LIMIT)) {
            std::copy(slot.obs.global.begin(), slot.obs.global.end(), g);
            std::copy(slot.obs.tokens.begin(), slot.obs.tokens.end(), t);
            std::copy(slot.obs.actions.begin(), slot.obs.actions.end(), a);
            counts[2 * i] = static_cast<int32_t>(slot.obs.token_count());
            counts[2 * i + 1] = static_cast<int32_t>(slot.obs.action_count());
        }
        status[i] = slot.status;
        steps[i] = slot.steps;
    }
    return 0;
}

void nosl_batch_run_info(void* handle, int32_t index, int32_t out[8]) {
    std::fill(out, out + 8, 0);
    Batch* batch = Cast(handle);
    if (!batch || index < 0 || static_cast<size_t>(index) >= batch->slots.size()) return;
    const Slot& slot = batch->slots[static_cast<size_t>(index)];
    if (!slot.env) return;
    const GameState& g = slot.env->state();
    if (!g.run.player.creature) return;
    out[0] = g.run.current_act_index;
    out[1] = g.run.total_floor;
    out[2] = g.creatures[g.run.player.creature].current_hp;
    out[3] = g.creatures[g.run.player.creature].max_hp;
    out[4] = g.run.player.gold;
    out[5] = static_cast<int32_t>(g.run.player.deck.size());
    out[6] = static_cast<int32_t>(g.run.player.relics.size());
    out[7] = slot.last_encounter;
}

void* nosl_policy_load(const char* path) {
    try {
        return nosl::PolicyNet::Load(path ? path : "").release();
    } catch (const std::exception& e) {
        g_policy_error = e.what();
        return nullptr;
    }
}

void nosl_policy_destroy(void* policy) { delete static_cast<nosl::PolicyNet*>(policy); }

const char* nosl_policy_last_error() { return g_policy_error.c_str(); }

int32_t nosl_batch_policy_forward(void* handle, int32_t index, void* policy, float* logits, int32_t capacity, float* value) {
    Batch* batch = Cast(handle);
    if (!batch || !policy || !logits || !value || index < 0 || static_cast<size_t>(index) >= batch->slots.size()) return -1;
    const Slot& slot = batch->slots[static_cast<size_t>(index)];
    if (slot.obs.action_count() > static_cast<size_t>(capacity)) return -1;
    std::vector<float> out;
    static_cast<nosl::PolicyNet*>(policy)->Forward(slot.obs, out, *value);
    std::copy(out.begin(), out.end(), logits);
    return static_cast<int32_t>(out.size());
}

const char* nosl_batch_last_error(void* handle, int32_t index) {
    Batch* batch = Cast(handle);
    if (!batch || index < 0 || static_cast<size_t>(index) >= batch->slots.size()) return "bad slot";
    return batch->slots[static_cast<size_t>(index)].error.c_str();
}

}
