#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define STS2_SOLVER_EXPORT __declspec(dllexport)
#else
#define STS2_SOLVER_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Independent search-consumer ABI; these are NOT ordinary Env handles.
// A session has one caller at a time. Borrowed UTF-8 strings remain valid until
// its next mutating call. Destroy only after Solve and all callbacks return.
enum sts2_solver_status {
    STS2_SOLVER_OK = 0,
    STS2_SOLVER_INVALID = 1,
    STS2_SOLVER_UNSUPPORTED = 2,
    STS2_SOLVER_CANCELLED = 3,
    STS2_SOLVER_ERROR = -1
};

typedef struct sts2_solver_config {
    uint32_t abi_version;
    uint32_t struct_size;
    int32_t beam_width;
    int32_t max_nodes;
    int32_t time_budget_ms;
    int32_t max_card_branches;
    int32_t max_pile_choice_branches;
    int32_t max_hand_choice_branches;
    int32_t threads;
    int32_t max_turns;
    int32_t max_actions_per_turn;
    int32_t potion_policy; // 0 disabled, 1 smart, 2 require at least one
    int32_t use_portfolio;
    int32_t stop_at_hp_target;
    int32_t transition_cache_mib;
} sts2_solver_config;

// Called synchronously from search workers, serialized by the solver.
// The callback must not throw, destroy the session, or re-enter its API.
typedef int32_t (*sts2_solver_cancel_callback)(void* context);

STS2_SOLVER_EXPORT uint32_t sts2_solver_abi_version(void);
STS2_SOLVER_EXPORT int32_t sts2_solver_default_config(sts2_solver_config* config, size_t size);
STS2_SOLVER_EXPORT void* sts2_solver_create(void);
STS2_SOLVER_EXPORT void sts2_solver_destroy(void* session);
// Returns private Mod capture field shapes and monster definitions, never a
// playable seeded state. The adapter must supply all live values and validate
// every returned monster definition before importing the assembled snapshot.
STS2_SOLVER_EXPORT int32_t sts2_solver_live_schema(void* session, const char* request, size_t size);
// Only complete validated snapshots are accepted. Failed import invalidates
// the previous root, so a subsequent search can never silently use stale data.
STS2_SOLVER_EXPORT int32_t sts2_solver_import_snapshot(void* session, const char* data, size_t size);
STS2_SOLVER_EXPORT int32_t sts2_solver_solve(void* session, const sts2_solver_config* config,
    sts2_solver_cancel_callback cancelled, void* context);
// Result contains display-only steps and replay-verified final facts.
// It deliberately provides no original-Mod deployment objects or callbacks.
STS2_SOLVER_EXPORT const char* sts2_solver_last_result(void* session);
STS2_SOLVER_EXPORT const char* sts2_solver_last_error(void* session);
STS2_SOLVER_EXPORT uint64_t sts2_solver_root_hash(void* session);

#ifdef __cplusplus
}
#endif
