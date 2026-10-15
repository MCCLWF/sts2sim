// C ABI of the NoSL sampler: a set of independent runs stepped by action-row
// index, with the actor observation written straight into caller buffers.
// One batch is single-threaded; callers parallelise with one batch per
// process, which also contains a native hang or crash to that process.
#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {

// Per-run status after reset/step. Anything but RUNNING needs a reset.
enum nosl_status {
    NOSL_RUNNING = 0,
    NOSL_VICTORY = 1,      // original terminal
    NOSL_DEATH = 2,        // original terminal
    NOSL_UNPORTED = 3,     // simulation reached unported content
    NOSL_ERROR = 4,        // any other failure; see nosl_batch_last_error
    NOSL_CAPACITY = 5,     // the observation does not fit the buffers
    NOSL_STEP_LIMIT = 6,   // sampling time limit on a legal trajectory
};

// Table shapes of schema nosl.obs v1: {schema, global width, token width, action width}.
void nosl_obs_shape(int32_t out[4]);

void* nosl_batch_create(int32_t runs, int32_t max_tokens, int32_t max_actions, int32_t max_steps);
void nosl_batch_destroy(void* batch);

// A new run for slot `index`, advanced to its first decision.
int32_t nosl_batch_reset(void* batch, int32_t index, const char* seed, int32_t character, int32_t ascension);

// Only the first `active` slots are stepped and written by nosl_batch_step;
// the rest are spares a caller can reset ahead of time and swap in.
int32_t nosl_batch_set_active(void* batch, int32_t active);
int32_t nosl_batch_swap(void* batch, int32_t a, int32_t b);
// Offline training only: copy a running source slot into a distinct destination.
// Env, public actor-filter history, current action table and step budget are
// owned independently. Returns destination status, or -1 on a bad argument.
// Does not advance either run or expose privileged state to observations.
int32_t nosl_batch_fork(void* batch, int32_t source, int32_t destination);
// Offline training only: resample what the player cannot see in a running slot that is inside a combat
// (nosl/resample.h: pile orders and the combat's random streams). The slot's observation does not change.
// Returns the slot's status, -1 on a bad argument, -2 when the slot is not in a combat or is at a choice inside
// an effect (nothing can be resampled there).
int32_t nosl_batch_resample(void* batch, int32_t index, uint64_t seed);
// The actor filter of runs reset from now on (nosl/actor_filter.h): 0 none, 1 version 1.
int32_t nosl_batch_set_filter(void* batch, int32_t version);

// For every active slot with choice[i] >= 0 and status RUNNING: take action row
// choice[i] of its current observation, then keep taking the only legal
// action while exactly one exists (auto_forced != 0). Then write every
// slot's observation. Buffers: global [runs x G], tokens [runs x max_tokens
// x W], actions [runs x max_actions x 4], counts [runs x 2] (tokens, actions),
// status [runs], steps [runs] (decisions taken in the run, forced included).
// Rows past a slot's counts are zeroed. Returns 0, or -1 on a bad argument.
int32_t nosl_batch_step(void* batch, const int32_t* choice, int32_t auto_forced,
                        int32_t* global, int32_t* tokens, int32_t* actions,
                        int32_t* counts, int32_t* status, int32_t* steps);

// The run's final facts for reporting: {act index, total floor, hp, max hp,
// gold, deck size, relic count, last combat encounter id}.
void nosl_batch_run_info(void* batch, int32_t index, int32_t out[8]);
const char* nosl_batch_last_error(void* batch, int32_t index);

// The native forward pass (nosl/policy_net.h) on weights written by
// python -m nosl.export. Load returns null on a bad file; the reason is in
// nosl_policy_last_error().
void* nosl_policy_load(const char* path);
void nosl_policy_destroy(void* policy);
const char* nosl_policy_last_error();
// Logits of slot `index`'s current action rows and its value estimate.
// Returns the action count, or -1 (bad argument, or `capacity` too small).
int32_t nosl_batch_policy_forward(void* batch, int32_t index, void* policy, float* logits, int32_t capacity, float* value);

}
