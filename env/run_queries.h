#pragma once

#include "env/env.h"
#include "engine/json.h"

namespace sts2 {

struct RunQueryOptions {
    // Explicit privileged forecast facts. Never exposes RNG, future room
    // queues or relic bags; this only adds the already selected next act.
    bool include_future = false;
};

// Owned JSON game facts for current rooms, deck, inventory and choices.
// Getter calls run on a private clone and may not advance gameplay/RNG.
// This is a consumer/forecast API, not the fixed agent observation encoding.
// Pool refs are scoped to the corresponding owned state, not global UIDs.
void WriteRunQueries(JsonWriter& writer, const Env& source, const RunQueryOptions& options = {});
std::string ExportRunQueries(const Env& source, const RunQueryOptions& options = {});

}  // namespace sts2
