#pragma once

#include "env/env.h"
#include "env/run_queries.h"
#include "engine/sim/execution_trace.h"

namespace sts2 {

struct PreviewOptions {
    bool collect_trace = true;
    size_t trace_limit = 16384;
    bool capture_run_results = false;
    RunQueryOptions run_query_options;
    bool collect_rng = false; // privileged diagnostic, never an agent observation
    bool collect_causal = false; // includes hidden listener identities/order
};

// An ordinary owned Env successor plus optional execution observations.
// Answer replays the entire pending operation from its stationary base;
// its trace replaces the preceding partial trace of that operation.
struct PreviewResult {
    std::unique_ptr<Env> successor;
    ExecutionTrace trace;
    Action action;
    std::optional<Operation> operation;
    std::optional<Operation> continuation_of;
    uint64_t source_hash = 0;
    bool trace_enabled = true;
    bool choice_replayed = false;
    // Optional owned source, independent of the caller's lifetime. For Answer
    // this is the current partial prefix, whereas trace replays from its base.
    std::unique_ptr<Env> result_source;
    RunQueryOptions run_query_options;

    std::string ExportJson() const;
};

PreviewResult PreviewAction(const Env& source, const Action& action,
                            const PreviewOptions& options = {});
PreviewResult PreviewOperation(const Env& source, const Operation& operation,
                               const PreviewOptions& options = {});

}  // namespace sts2
