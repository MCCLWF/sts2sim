#pragma once

#include <cstddef>
#include <cstdint>
#include "env/api_descriptor.h"

extern "C" {

struct sts2_action {
    int32_t kind;
    int32_t index;
    int32_t target;
    int32_t col;
    int32_t row;
};

// Status for reset, inspect, transition, preview, step and determinize: 0 =
// success, 1 = simulation reached unported content, 2 = preview trace capacity
// exceeded (no successor), 3 = unsupported operation, 4 = resource/type capacity,
// 5 = invalid operation request, -1 = another error.
// legal_actions, action_mask and observe return a nonnegative count instead.
// On failure, read sts2_env_last_error(handle). Never search a failed branch.
enum sts2_env_status {
    STS2_ENV_OK = 0,
    STS2_ENV_UNPORTED = 1,
    STS2_ENV_TRACE_LIMIT = 2,
    STS2_ENV_UNSUPPORTED = 3,
    STS2_ENV_CAPACITY = 4,
    STS2_ENV_INVALID_REQUEST = 5,
    STS2_ENV_ERROR = -1,
};

void* sts2_env_create();
void* sts2_env_clone(void* handle);
void sts2_env_destroy(void* handle);
int sts2_env_reset(void* handle, const char* seed, int32_t character, int32_t ascension);
int sts2_env_reset_named(void* handle, const char* seed, const char* character, int32_t ascension);
int sts2_env_reset_combat(void* handle, const char* seed, int32_t character, int32_t ascension,
                          int32_t encounter, int32_t act_index);
int sts2_env_inspect_combat_named(void* handle, const char* seed, const char* character,
                                 int32_t ascension, const char* encounter, int32_t act_index);
// Inspect the current state without resetting it. Combat states include a
// structural root; all states include the decision kind and state hash.
int sts2_env_inspect_current(void* handle);
// Diagnostic combat state in the existing OracleCombat.Dump format.
int sts2_env_export_oracle_combat_state(void* handle);
// Diagnostic current run state in the existing OracleRun.DumpRun format.
int sts2_env_export_oracle_run_state(void* handle);
// Diagnostic active reward entries (ordinary or nested choice):
// OracleRun.RewardJson content plus selected status.
int sts2_env_export_oracle_rewards(void* handle);
// Privileged live diagnostic for the active event's private RNG stream.
int32_t sts2_env_event_rng_counter(void* handle);
// Full versioned logical state, including hidden RNG and choice replay base.
// JSON is returned through last_result; this is not the agent observation API.
int sts2_env_export_snapshot(void* handle);
// Canonical cross-process snapshot. Import requires the identical native build
// fingerprint; old local snapshots keep their loaded-runtime restriction.
int sts2_env_export_portable_snapshot(void* handle);
// Parse and validate a local version-4 snapshot from this loaded runtime, or
// portable version-1 state from an identical native build. On success the caller
// owns *successor; every failure sets it
// to null. diagnostic_handle's simulation state is unchanged. data may alias
// last_result; the function copies it before replacing diagnostic buffers.
int sts2_env_import_snapshot(void* diagnostic_handle, const char* data, size_t length, void** successor);
// Reject an active combat decision/choice containing a model with unported overrides.
// This is conservative root coverage, not proof that all future generated
// content is ported; transitions still return STS2_ENV_UNPORTED if reached.
int sts2_env_validate_combat_root(void* handle);
// Pure one-action prediction. Combat sources and successors are checked for
// unported models. The source is never modified. On success the caller owns
// *successor and must destroy it; on any failure *successor is null.
int sts2_env_try_transition(void* source, sts2_action action, void** successor);
// Pure ordinary execution with an optional factual trace. trace_limit=0
// disables collection; otherwise 1..1000000. The owned successor is a normal
// Env handle and holds sts2env.preview v1 JSON through last_result. Failure
// leaves it null and source gameplay state unchanged. A Choice answer returns
// the entire replayed operation prefix, replacing the prior partial trace.
int sts2_env_preview(void* source, sts2_action action, int32_t trace_limit, void** successor);
// Preview v2 adds optional owned before/after run queries. query_flags bit 0
// requests privileged future facts; bit 1 requests privileged RNG call
// diagnostics and requires a nonzero trace_limit. Run-query flags remain 0/1.
// Bit 2 requests privileged model/Hook call frames (including hidden listener
// identities/order), also requiring nonzero trace_limit. Only bits 0..2 accepted.
// Same execution and
// failure/ownership contract as preview; refs are scoped to each owned state.
int sts2_env_preview_with_results(void* source, sts2_action action, int32_t trace_limit,
                                  int32_t query_flags, void** successor);
// Versioned typed native command request; see sts2_env_api_descriptor.
// Binds all instance refs to source_hash. Pure, owned successor, no successor
// on failure. trace_limit=0 disables collection; query_flags as above.
int sts2_env_predict_operation(void* source, const char* data, size_t length,
                               int32_t trace_limit, int32_t query_flags, void** successor);
// Current run/room/choice facts on a private clone, returned through last_result.
// flags=0 by default, 1 explicitly includes privileged next-act facts. Other
// values fail. This consumer query is not the agent observation encoding.
int sts2_env_export_run_queries(void* handle, int32_t flags);
// Shipped combat getters on a private clone; query JSON uses last_result.
// These facts and formula operands contain no client scoring or filtering.
int sts2_env_export_combat_queries(void* handle);
int sts2_env_export_calculated_vars(void* handle);
int sts2_env_export_orbs(void* handle);
int sts2_env_export_powers(void* handle);
const char* sts2_env_last_result(void* handle);
int32_t sts2_env_decision(void* handle);
uint64_t sts2_env_hash(void* handle);
int sts2_env_determinize(void* handle, uint64_t seed);
int sts2_env_step(void* handle, sts2_action action);
// Each count query accepts (out=nullptr, capacity=0) to discover the required
// element count. Then supply a buffer of that size; undersized buffers are
// rejected before writing. The handle must not be mutated between calls.
int32_t sts2_env_legal_actions(void* handle, sts2_action* out, size_t capacity);
int32_t sts2_env_action_mask(void* handle, uint8_t* out, size_t capacity);
int32_t sts2_env_observe(void* handle, int32_t* out, size_t capacity);
const char* sts2_env_last_error(void* handle);

}
